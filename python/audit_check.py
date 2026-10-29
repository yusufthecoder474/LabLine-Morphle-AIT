#!/usr/bin/env python3
"""Independent record-consistency check. Missing records do not prove skipped actions."""
import argparse
import csv
import json
from pathlib import Path

STATES = ['RECEIVED', 'STAINED', 'HEATED', 'PLACED', 'IMAGED', 'DONE']


def check(path, summary=None):
    errors, jobs, requests, acknowledged = [], {}, {}, set()
    run_id, previous_time = None, -1
    with open(path, newline='') as stream:
        rows = list(csv.DictReader(stream))
    for line, row in enumerate(rows, 2):
        def error(message):
            errors.append(f'line {line}: {message}')
        try:
            job, seq = int(row['job_id']), int(row['sequence'])
            timestamp = int(row['steady_elapsed_ns'])
            event, result = row['event'], row['result']
            if run_id is None:
                run_id = row['run_id']
            if row['run_id'] != run_id:
                error('mixed run IDs')
            if timestamp < previous_time:
                error('out-of-order monotonic timestamp')
            previous_time = timestamp
            if job == 0 and event == 'CRC_REJECT':
                continue
            if event == 'ACCEPT':
                if job in jobs:
                    error('duplicate accepted job')
                jobs[job] = {'state': 'RECEIVED', 'terminal': 0, 'unknown': False}
                continue
            if job not in jobs:
                error('unknown job ID')
                continue
            current = jobs[job]
            if current['terminal']:
                error('event after terminal outcome')
            if event == 'REQUEST':
                if seq in requests:
                    error('duplicate request ID')
                requests[seq] = (job, row['device'], result)
            elif event in ('ACK', 'NACK', 'ERROR', 'RETRY'):
                if seq not in requests or requests[seq][:2] != (job, row['device']):
                    error('response/retry without matching request')
                if event == 'ACK':
                    if seq in acknowledged:
                        error('duplicate ACK')
                    acknowledged.add(seq)
            elif event == 'STATE':
                before = current['state']
                if before in ('DONE', 'FAILED'):
                    error('transition after final state')
                elif result != 'FAILED':
                    expected = STATES[STATES.index(before) + 1]
                    if result != expected:
                        error(f'missing or out-of-order transition: expected {expected}; recorded {result}; execution unproven')
                    if result in STATES[1:5]:
                        op = str(STATES.index(result))
                        if not any(requests[s][0] == job and requests[s][2] == op for s in acknowledged):
                            error('transition without confirmed command ACK')
                current['state'] = result
            elif event == 'TERMINAL':
                current['terminal'] += 1
                current['unknown'] = row['reason'] == 'UNKNOWN'
                if current['terminal'] != 1:
                    error('duplicate terminal completion')
                if result not in ('DONE', 'FAILED') or result != current['state']:
                    error('terminal/state mismatch')
            else:
                error('unrecognized event')
        except (ValueError, KeyError, IndexError) as exc:
            error(f'malformed audit row: {exc}')
    for job, record in jobs.items():
        if record['terminal'] != 1:
            errors.append(f'job {job}: unfinished or duplicate terminal accepted job')
    totals = {state.lower(): sum(j['state'] == state for j in jobs.values()) for state in ('DONE', 'FAILED')}
    totals['unknown'] = sum(j['unknown'] for j in jobs.values())
    if not jobs:
        errors.append('no accepted jobs')
    if summary:
        with open(summary, newline='') as stream:
            s = next(csv.DictReader(stream))
        if int(s['jobs']) != len(jobs) or set(jobs) != set(range(1, int(s['jobs']) + 1)):
            errors.append('accepted ID set inconsistent with summary')
        if any(int(s[key]) != value for key, value in totals.items()):
            errors.append('inconsistent success/failure/unknown totals')
        if run_id != s['run_id']:
            errors.append('summary run ID mismatch')
    return {'accepted': len(jobs), **totals, 'anomalies': len(errors), 'errors': errors,
            'interpretation': 'Missing audit evidence is not proof that a simulated action was skipped.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('events', type=Path)
    parser.add_argument('--summary', type=Path)
    args = parser.parse_args()
    result = check(args.events, args.summary)
    print(json.dumps(result, indent=2))
    raise SystemExit(bool(result['anomalies']))
