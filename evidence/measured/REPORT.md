# LabLine measured experiment report

Captured UTC: 2026-10-29T07:24:06.946924+00:00

Synthetic devices and software-only Linux IPC. Unknown is a subset of failed, not an extra job count.

## Matched no-fault workloads

Three paired repetitions use seeds 41, 42, 43 for each size. Entries below are arithmetic means of per-run measurements.

| Jobs | Workers | Runs | DONE / accepted | Mean throughput jobs/s | Mean job p95 ms | Mean job p99 ms | Mean IPC p95 ms |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 10 | 1 | 3 | 30 / 30 | 37.74 | 264.71 | 264.71 | 7.72 |
| 10 | 4 | 3 | 30 / 30 | 69.38 | 143.73 | 143.73 | 7.80 |
| 50 | 1 | 3 | 150 / 150 | 37.51 | 270.00 | 270.29 | 7.75 |
| 50 | 4 | 3 | 150 / 150 | 73.21 | 203.44 | 216.94 | 7.84 |
| 100 | 1 | 3 | 300 / 300 | 38.00 | 268.03 | 268.88 | 7.68 |
| 100 | 4 | 3 | 300 / 300 | 74.44 | 202.36 | 220.40 | 7.76 |

## Injected faults

| Fault | DONE | FAILED | UNKNOWN subset | Retries | CRC rejects | Deadline misses | Audit anomalies |
|---|---:|---:|---:|---:|---:|---:|---:|
| ack | 10 | 0 | 0 | 1 | 0 | 0 | 0 |
| crc | 10 | 0 | 0 | 1 | 1 | 0 | 0 |
| slow | 10 | 0 | 0 | 0 | 0 | 7 | 0 |
| crash | 0 | 10 | 2 | 0 | 0 | 0 | 0 |
| timeout | 9 | 1 | 1 | 2 | 0 | 1 | 0 |

## Definitions and limits

- Job response: admission timestamp through terminal state preparation; includes bounded queue backpressure, queue wait and station wait. Terminal log write is outside the job timer.
- Throughput: all accepted jobs divided by admission-to-workers-joined wall time. For faults this is accounting throughput, not successful output throughput.
- IPC response: first send attempt to validated ACK/NACK, including retry wait. Timeouts without responses are excluded from IPC percentiles and counted as failures/unknown.
- Percentiles: nearest rank ceil(p*N), separately for each run. Jobs supply N=10/50/100; successful command samples N=4*jobs without faults. p99 at N=10 or 50 is the maximum.
- Jitter: population standard deviation of all measured command response times in a run, across mixed operations. It is not periodic timer jitter or a hard deadline guarantee.
- Soft deadline: 200 ms from admission. Bounded queue admission backpressure means this is a streaming workload, not 100 simultaneous arrivals.
- Simulator duration formula depends on seed, job and operation; pump assignment and event interleaving can vary. Timing is not deterministic.
- CSV logging and flushes are enabled in both modes; overhead is included and was not separately measured. Host scheduling and shared-machine load are uncontrolled.
- Serial and concurrent modes use the same safe code, devices, delays and logs. No causal clinical or hardware conclusions can be drawn.
- Independent device START/COMPLETE intervals verify actual overlap. No-fault runs are checked for exclusive use, duplicate actions and complete accounting.
- Audit anomalies measure record consistency, not independent proof of physical execution. No production uptime or financial benefit measured.
