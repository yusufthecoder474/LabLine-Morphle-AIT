#!/usr/bin/env python3
import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from audit_check import check


def read(path):
    with open(path, newline='') as f:
        return list(csv.DictReader(f))


def simulation(binary, folder, fault='none', jobs=10, workers=4):
    command = [binary, '--out', str(folder), '--fault', fault, '--jobs', str(jobs),
               '--workers', str(workers), '--run-id', fault]
    run = subprocess.run(command, capture_output=True, text=True, timeout=15)
    assert run.returncode == (2 if fault in ('crash', 'timeout') else 0), run.stdout + run.stderr
    audit = check(folder / 'events.csv', folder / 'summary.csv')
    assert audit['anomalies'] == 0, audit
    return read(folder / 'summary.csv')[0], read(folder / 'simulator.csv')


def inspect_actions(rows, allow_incomplete=False):
    active, started, completed, max_active = set(), set(), set(), 0
    for row in rows:
        seq, device, event = row['sequence'], row['device'], row['event']
        if event == 'START':
            assert device not in active, 'exclusive device overlap'
            assert not (device == '1' and '4' in active or device == '4' and '1' in active), 'arm/heater reservation overlap'
            assert seq not in started, 'duplicate action'
            active.add(device)
            started.add(seq)
            max_active = max(max_active, len(active))
        if event == 'COMPLETE':
            assert seq in started and seq not in completed
            active.remove(device)
            completed.add(seq)
    if not allow_incomplete:
        assert started == completed and not active
    return max_active, len(completed)


def test(binary, name, folder):
    if name == 'deadlock':
        demo = str(Path(binary).with_name('deadlock_demo'))
        try:
            subprocess.run([demo, '--demo-deadlock=unsafe'], capture_output=True, text=True, timeout=1)
            raise AssertionError('unsafe demo unexpectedly completed')
        except subprocess.TimeoutExpired as exc:
            assert (exc.stdout or b'').count(b'first_lock_held') == 2, exc.stdout
            print('unsafe: both first locks held; externally killed after 1 s')
        safe = subprocess.run([demo, '--demo-deadlock=safe'], capture_output=True, text=True, timeout=3)
        assert safe.returncode == 0 and 'safe completed' in safe.stdout
        return
    fault = name if name in ('ack', 'crc', 'slow', 'crash', 'timeout') else 'none'
    summary, actions = simulation(binary, folder, fault)
    overlap, completed = inspect_actions(actions, fault in ('crash', 'timeout'))
    if name == 'concurrency':
        assert overlap >= 2, 'independent operations did not overlap'
        assert completed == 40 and int(summary['done']) == 10
    elif name in ('ack', 'crc'):
        assert int(summary['retries']) >= 1 and completed == 40
        assert any(r['event'] == 'DEDUP' for r in actions)
        if name == 'crc':
            assert int(summary['crc_rejects']) == 1
    elif name == 'slow':
        baseline, _ = simulation(binary, folder / 'baseline')
        assert int(summary['deadline_misses']) > int(baseline['deadline_misses'])
        assert float(summary['p95_ipc_ms']) > float(baseline['p95_ipc_ms']) + 20
    elif name in ('crash', 'timeout'):
        assert int(summary['failed']) > 0 and int(summary['unknown']) > 0
        if name == 'timeout':
            assert int(summary['retries']) == 2
    elif name == 'repeat':
        _, other = simulation(binary, folder / 'repeat')
        signature = lambda rows: sorted((r['job_id'], r['operation']) for r in rows if r['event'] == 'COMPLETE')
        assert signature(actions) == signature(other)
    elif name == 'audit':
        rows = read(folder / 'events.csv')
        variants = {}
        index = next(i for i, r in enumerate(rows) if r['event'] == 'STATE' and r['result'] == 'STAINED')
        variants['missing'] = rows[:index] + rows[index + 1:]
        reordered = [dict(r) for r in rows]
        reordered[index]['result'] = 'IMAGED'
        variants['out_of_order'] = reordered
        terminal = next(r for r in rows if r['event'] == 'TERMINAL')
        variants['duplicate'] = rows + [terminal]
        unknown = dict(terminal, job_id='999')
        variants['unknown'] = rows + [unknown]
        variants['unfinished'] = [r for r in rows if not (r['event'] == 'TERMINAL' and r['job_id'] == '1')]
        for label, altered in variants.items():
            path = folder / f'negative_{label}.csv'
            with open(path, 'w', newline='') as f:
                w = csv.DictWriter(f, fieldnames=rows[0].keys()); w.writeheader(); w.writerows(altered)
            assert check(path, folder / 'summary.csv')['anomalies'] > 0, label
        wrong = dict(summary, done='0')
        path = folder / 'negative_summary.csv'
        with open(path, 'w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=wrong.keys()); w.writeheader(); w.writerow(wrong)
        assert check(folder / 'events.csv', path)['anomalies'] > 0
    print(json.dumps({'test': name, 'overlap': overlap, 'completed_actions': completed, 'summary': summary}))


if __name__ == '__main__':
    with tempfile.TemporaryDirectory(prefix='labline-test-') as directory:
        test(str(Path(sys.argv[1]).resolve()), sys.argv[2], Path(directory))
    print(sys.argv[2], 'PASS')
