# Injected failures: observed behavior and mitigation

These are intentionally injected local experiments, not production incidents. Run-specific evidence lives under `evidence/measured/`. See `REPORT.md` for actual counts and timestamps.

| Fault and reproduction | Root cause in this simulator | Evidence and mitigation | Remaining limit |
|---|---|---|---|
| `--fault ack` | Job 1's stain completes; its first reply is intentionally discarded | Simulator COMPLETE then ACK_DROPPED then DEDUP; parent RETRY then ACK. Same sequence produces one action. `integration_ack` and simulator input test pass only with duplicate-free completion. | In-memory dedup only; cannot infer across crash/restart |
| `--fault crc` | One reply payload bit is flipped after CRC calculation | Parent CRC_REJECT; no workflow transition from corrupted reply; retry returns cached valid reply. `integration_crc`; independent bad-request probe verifies no action from a corrupt input frame. | CRC detects corruption; no authentication or adversarial security guarantee |
| `--fault slow` | Heater adds 45 ms service delay per operation | Larger IPC p95 and more 200 ms soft deadline misses than matched baseline; terminal counts still reconcile. `integration_slow` compares real runs. | Queues cannot remove physical service time; no hard real-time guarantee |
| `--fault crash` | Child exits at first targeted stain before confirmation | Peer disconnect resolves pending futures, affected jobs fail/unknown, later jobs fail not-issued, parent reaps child. `integration_crash` has external 15 s bound. | No automatic restart or exactly-once across process loss |

Additional boundary case `--fault timeout` suppresses every reply for the targeted sequence. The action completes once, two retries are observed, and the controller aborts the run with UNKNOWN for unconfirmed requests. This extends the required ACK-loss scenario rather than adding a new device failure model.

## Audit negative controls

The audit test creates separate altered CSVs for missing transition, out-of-order transition, duplicate terminal, unknown job, unfinished job and wrong summary totals. The genuine events file is retained unchanged. A missing transition is reported as missing record evidence; it is not declared a skipped physical operation. `evidence/negative-controls/` retains executed examples and checker outputs.

## Lock inversion

The unsafe demonstration proves both threads hold their first lock before either requests the other. External process termination is the expected observation. The safe version applies one global order. Neither result establishes a production incident or claims an already-deadlocked mutex was repaired.

## Toolchain investigation during this build

Initial TSan tests reported unlock-of-unlocked-mutex around `std::timed_mutex::try_lock_until`. A standalone one-mutex, single-thread probe reproduced the diagnostic (`evidence/initial/timed_mutex_probe.cpp` and `.log`). This supports a toolchain/interception incompatibility rather than a multiworker ownership failure; it is not a proof about the sanitizer's internals. Station locking now uses `std::mutex`, `try_lock`, RAII and an explicit steady-clock deadline. No suppression was added. Final TSan CTest passed 17/17.

ASan+UBSan compilation succeeded. Its default CTest run failed when LeakSanitizer could not inspect `/proc/.../task` and reported a ptrace-related limitation. The original failures are retained. Leak detection was not disabled; this configuration is ENVIRONMENT BLOCKED and is not claimed sanitizer-clean.
