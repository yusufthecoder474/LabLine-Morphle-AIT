
# LabLine — Requirements Traceability

## Purpose

This document maps LabLine's software requirements to existing
automated tests and recorded verification evidence.

LabLine is an educational C++17 actuator-controller simulator
using Linux inter-process communication (IPC), concurrent
workers and a simulated device protocol.

It is not certified medical, industrial or aerospace software.
Passing automated tests does not establish hard real-time
performance or functional-safety certification.

## System overview

The controller accepts jobs, places them in a bounded queue
and processes them through worker threads.

A separate simulator process communicates through a Unix
socketpair. The simulated devices include pumps, a heater,
a handling arm and a camera.

The workflow is:

RECEIVED → STAINED → HEATED → PLACED → IMAGED → DONE

A job can transition to FAILED when an operation cannot
complete successfully.

See [DESIGN.md](DESIGN.md) for architecture and ownership
of shared mutable state.

## Requirements-to-test matrix

| ID | Requirement | Existing verification |
|---|---|---|
| REQ-01 | The controller shall enforce the bounded job queue and support queue closure. | `unit_queue` |
| REQ-02 | Jobs shall follow valid workflow-state transitions. | `unit_state` |
| REQ-03 | Concurrent workers shall coordinate station access without overlapping incompatible operations. | `integration_concurrency` |
| REQ-04 | Lost acknowledgements shall be handled through retry and simulator-side deduplication. | `integration_ack` |
| REQ-05 | Invalid CRC values shall be rejected without accepting a corrupted reply as a valid transition. | `unit_crc`, `integration_crc` |
| REQ-06 | The controller shall handle unavailable or delayed acknowledgements with bounded timeout behavior. | `integration_timeout`, `integration_slow` |
| REQ-07 | The controller shall handle simulator process termination without hanging indefinitely. | `integration_crash` |
| REQ-08 | Audit records shall be checked for valid transitions, terminal events and consistent summaries. | `integration_audit` |
| REQ-09 | Station locking shall use a consistent acquisition order to reduce deadlock risk. | `integration_deadlock` |
| REQ-10 | Device-protocol input validation shall reject invalid frames and unsupported inputs. | `simulator_input_safety`, `unit_length`, `unit_version` |
| REQ-11 | Frame serialization and parsing shall support round-trip and split/multiple-frame cases. | `unit_roundtrip`, `unit_split_multiple` |
| REQ-12 | Repeated requests shall be handled consistently within the simulator's deduplication scope. | `integration_repeat` |

Test names are taken from the repository's `CMakeLists.txt`.

## Test inventory

The repository defines 17 CTest cases.

### Unit tests — 7

- `unit_roundtrip`
- `unit_split_multiple`
- `unit_crc`
- `unit_length`
- `unit_version`
- `unit_state`
- `unit_queue`

### Integration tests — 9

- `integration_concurrency`
- `integration_ack`
- `integration_crc`
- `integration_slow`
- `integration_crash`
- `integration_timeout`
- `integration_repeat`
- `integration_audit`
- `integration_deadlock`

### Protocol safety — 1

- `simulator_input_safety`

The requirements matrix identifies relevant verification cases.
It does not establish full requirements coverage, and each
test's exact assertions should be checked in its source file.

## Recorded verification evidence

The existing repository evidence records:

- Release configuration: 17/17 CTest cases passed.
- Debug configuration: 17/17 CTest cases passed.
- ThreadSanitizer configuration: final 17/17 CTest cases passed.
- Controlled evaluation: 960/960 jobs accepted.
- Serial throughput: 38.00 jobs per second.
- Concurrent throughput: 74.44 jobs per second.
- Serial p95 latency: 268.03 ms.
- Concurrent p95 latency: 202.36 ms.

These values belong to the recorded environments and workloads.
They are not guarantees for other hardware, operating systems
or workloads.

Evidence is retained in:

- `evidence/REPORT.md`
- `evidence/REPORT_2026-11-02.md`
- `evidence/VERIFICATION.md`
- `evidence/WSL2_REPORT_2026-10-29.md`

## Fault handling

LabLine includes deliberately injected failure scenarios:

| Fault | Behavior under test | Relevant test |
|---|---|---|
| Dropped ACK | Retry and deduplication after a missing reply | `integration_ack` |
| Corrupted reply | CRC rejection and recovery | `integration_crc` |
| Slow device | Delayed processing and deadline observations | `integration_slow` |
| Simulator crash | Peer disconnect handling and child cleanup | `integration_crash` |
| Suppressed replies | Timeout and unknown-outcome handling | `integration_timeout` |

See [ROOT_CAUSE_REPORTS.md](ROOT_CAUSE_REPORTS.md).

## Sanitizer verification and limitations

### ThreadSanitizer

The initial ThreadSanitizer investigation exposed a
`std::timed_mutex` diagnostic that was also reproduced
in a standalone probe.

Station locking was revised to use `std::mutex`,
`try_lock`, RAII and an explicit steady-clock deadline.

The final recorded ThreadSanitizer CTest run passed 17/17.

This does not prove the absence of every possible race.

### AddressSanitizer and UndefinedBehaviorSanitizer

The ASan/UBSan configuration compiled successfully.

The default test execution encountered a
LeakSanitizer environment limitation involving `/proc`
and ptrace access.

This configuration must not be reported as fully passed.
The original failure logs are retained under `evidence/`.

## Known limitations

1. LabLine is a simplified educational simulator,
   not a validated physical actuator controller.
2. The timing measurements do not establish hard
   real-time scheduling guarantees.
3. Simulator deduplication is in memory and does not
   provide exactly-once execution across process restarts.
4. Lock-ordering tests do not prove that deadlock is
   impossible in every execution.
5. Tests and sanitizers cannot establish complete absence
   of concurrency defects.
6. The project has not undergone DO-178C certification,
   formal safety assessment or production qualification.

## Software development workflow

This traceability document was added as a planned
documentation improvement associated with GitHub Issue #1.

Issue:
https://github.com/yusufthecoder474/LabLine-Morphle-AIT/issues/1

The change is intended to be reviewed through a GitHub
Pull Request before merging into `main`.

## References

- [System design](DESIGN.md)
- [Root-cause reports](ROOT_CAUSE_REPORTS.md)
- [Build commands](BUILD_COMMANDS.md)
- [Verification evidence](../evidence/VERIFICATION.md)
- [CTest definitions](../CMakeLists.txt)
