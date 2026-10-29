#!/usr/bin/env python3
"""Matched synthetic workloads. All published numbers come from subprocess output."""
import argparse
import csv
import datetime as dt
import json
import platform
import statistics
import subprocess
import sys
from pathlib import Path
from audit_check import check
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tests'))
from integration import inspect_actions, read


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', default='build/labline', type=Path)
    parser.add_argument('--output', default='evidence/local', type=Path)
    args = parser.parse_args()
    binary, output = args.binary.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    environment = {'utc': dt.datetime.now(dt.timezone.utc).isoformat(), 'platform': platform.platform(),
                   'python': sys.version, 'binary': str(binary), 'command': sys.argv}
    (output / 'environment.json').write_text(json.dumps(environment, indent=2))
    records = []
    scenarios = [(n, workers, seed, 'none') for n in (10, 50, 100) for seed in (41, 42, 43) for workers in (1, 4)]
    scenarios += [(10, 4, 42, fault) for fault in ('ack', 'crc', 'slow', 'crash', 'timeout')]
    for n, workers, seed, fault in scenarios:
        run_id = f'n{n}_w{workers}_s{seed}_{fault}'
        folder = output / run_id
        folder.mkdir(exist_ok=True)
        command = [str(binary), '--jobs', str(n), '--workers', str(workers), '--seed', str(seed),
                   '--fault', fault, '--out', str(folder), '--run-id', run_id]
        started = dt.datetime.now(dt.timezone.utc).isoformat()
        result = subprocess.run(command, capture_output=True, text=True, timeout=30)
        (folder / 'stdout.log').write_text(result.stdout)
        (folder / 'stderr.log').write_text(result.stderr)
        (folder / 'command.json').write_text(json.dumps({'command': command, 'utc': started, 'exit_code': result.returncode}, indent=2))
        assert result.returncode == (2 if fault in ('crash', 'timeout') else 0), result.stderr
        audit = check(folder / 'events.csv', folder / 'summary.csv')
        (folder / 'audit.json').write_text(json.dumps(audit, indent=2))
        assert not audit['anomalies'], audit
        row = read(folder / 'summary.csv')[0]
        overlap, actions = inspect_actions(read(folder / 'simulator.csv'), fault in ('crash', 'timeout'))
        row.update(audit_anomalies=audit['anomalies'], max_overlapping_devices=overlap, completed_actions=actions)
        if fault == 'none':
            assert int(row['done']) == n and actions == n * 4
            assert overlap == 1 if workers == 1 else overlap >= 2
        if fault in ('ack', 'crc'):
            assert int(row['retries']) >= 1 and actions == 40
        if fault == 'crc':
            assert int(row['crc_rejects']) == 1
        if fault == 'slow':
            assert int(row['deadline_misses']) > 0
        if fault in ('crash', 'timeout'):
            assert int(row['failed']) > 0 and int(row['unknown']) > 0
        records.append(row)
        print(result.stdout.strip(), flush=True)
    with open(output / 'results.csv', 'w', newline='') as f:
        writer = csv.DictWriter(f, fieldnames=records[0].keys()); writer.writeheader(); writer.writerows(records)
    report = ['# LabLine measured experiment report', '', f"Captured UTC: {environment['utc']}", '',
              'Synthetic devices and software-only Linux IPC. Unknown is a subset of failed, not an extra job count.', '',
              '## Matched no-fault workloads', '',
              'Three paired repetitions use seeds 41, 42, 43 for each size. Entries below are arithmetic means of per-run measurements.', '',
              '| Jobs | Workers | Runs | DONE / accepted | Mean throughput jobs/s | Mean job p95 ms | Mean job p99 ms | Mean IPC p95 ms |',
              '|---:|---:|---:|---:|---:|---:|---:|---:|']
    for n in (10, 50, 100):
        for workers in (1, 4):
            group = [r for r in records if int(r['jobs']) == n and int(r['workers']) == workers and r['fault'] == 'none']
            avg = lambda key: statistics.mean(float(r[key]) for r in group)
            report.append(f"| {n} | {workers} | 3 | {sum(int(r['done']) for r in group)} / {n*3} | {avg('throughput'):.2f} | {avg('p95_ms'):.2f} | {avg('p99_ms'):.2f} | {avg('p95_ipc_ms'):.2f} |")
    report += ['', '## Injected faults', '', '| Fault | DONE | FAILED | UNKNOWN subset | Retries | CRC rejects | Deadline misses | Audit anomalies |',
               '|---|---:|---:|---:|---:|---:|---:|---:|']
    for row in records:
        if row['fault'] != 'none':
            report.append('| ' + ' | '.join(str(row[k]) for k in ('fault', 'done', 'failed', 'unknown', 'retries', 'crc_rejects', 'deadline_misses', 'audit_anomalies')) + ' |')
    report += ['', '## Definitions and limits', '',
               '- Job response: admission timestamp through terminal state preparation; includes bounded queue backpressure, queue wait and station wait. Terminal log write is outside the job timer.',
               '- Throughput: all accepted jobs divided by admission-to-workers-joined wall time. For faults this is accounting throughput, not successful output throughput.',
               '- IPC response: first send attempt to validated ACK/NACK, including retry wait. Timeouts without responses are excluded from IPC percentiles and counted as failures/unknown.',
               '- Percentiles: nearest rank ceil(p*N), separately for each run. Jobs supply N=10/50/100; successful command samples N=4*jobs without faults. p99 at N=10 or 50 is the maximum.',
               '- Jitter: population standard deviation of all measured command response times in a run, across mixed operations. It is not periodic timer jitter or a hard deadline guarantee.',
               '- Soft deadline: 200 ms from admission. Bounded queue admission backpressure means this is a streaming workload, not 100 simultaneous arrivals.',
               '- Simulator duration formula depends on seed, job and operation; pump assignment and event interleaving can vary. Timing is not deterministic.',
               '- CSV logging and flushes are enabled in both modes; overhead is included and was not separately measured. Host scheduling and shared-machine load are uncontrolled.',
               '- Serial and concurrent modes use the same safe code, devices, delays and logs. No causal clinical or hardware conclusions can be drawn.',
               '- Independent device START/COMPLETE intervals verify actual overlap. No-fault runs are checked for exclusive use, duplicate actions and complete accounting.',
               '- Audit anomalies measure record consistency, not independent proof of physical execution. No production uptime or financial benefit measured.', '']
    (output / 'REPORT.md').write_text('\n'.join(report))
    print(f'Saved {len(records)} measured runs: {output / "REPORT.md"}')


if __name__ == '__main__':
    main()
