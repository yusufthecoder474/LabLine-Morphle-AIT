# LabLine — Safety-Oriented Testing and Reliability

## Purpose

This document explains the defensive programming, concurrency testing and fault-handling practices used in LabLine.

LabLine is an educational C++17 actuator-controller simulator. It is not certified aerospace, medical or industrial control software.

## Concurrency safety

The controller uses concurrent workers, bounded queues, mutexes and condition variables to coordinate simulated actuator jobs.

Station locks are acquired in a consistent order to reduce deadlock risk. RAII is used to release resources when operations complete or fail.

Relevant automated tests:

- `integration_concurrency`
- `integration_deadlock`
- `unit_queue`

## ThreadSanitizer verification

An initial ThreadSanitizer investigation produced a diagnostic involving `std::timed_mutex`.

A standalone probe reproduced the diagnostic, supporting a possible toolchain/interception issue.

The station-locking implementation was changed to use `std::mutex`, `try_lock`, RAII and an explicit steady-clock deadline.

The final recorded ThreadSanitizer CTest run passed **17/17 tests**.

This result does not prove the absence of all concurrency defects.

## AddressSanitizer and UndefinedBehaviorSanitizer

The ASan/UBSan configuration compiled successfully.

The default test execution encountered a LeakSanitizer environment limitation involving `/proc` and ptrace access.

The ASan/UBSan run is therefore **not reported as fully passed**.

Original diagnostic logs are retained in the `evidence/` directory.

## Fault-injection testing

| Fault | Expected defensive behavior | Existing test |
|---|---|---|
| Dropped acknowledgement | Retry without duplicate simulated action | `integration_ack` |
| Corrupted reply | Reject invalid CRC and avoid incorrect state transition | `integration_crc` |
| Delayed device response | Observe deadline misses and maintain consistent terminal accounting | `integration_slow` |
| Simulator crash | Resolve pending requests and terminate without hanging indefinitely | `integration_crash` |
| Suppressed replies | Record unknown outcomes after bounded retries | `integration_timeout` |

These failures were deliberately injected for controlled testing. They are not production incidents.

## Verification evidence

The repository records:

- Release: 17/17 CTest cases passed.
- Debug: 17/17 CTest cases passed.
- ThreadSanitizer: 17/17 CTest cases passed.
- Controlled evaluation: 960/960 jobs accepted.

The results apply to the recorded environments and workloads.

## Engineering limitations

- No DO-178C certification or formal aerospace safety assessment.
- No hard real-time scheduling guarantee.
- No exactly-once execution guarantee across simulator restarts.
- No complete proof that deadlock or data races are impossible.
- No production hardware validation.

## References

- [System design](DESIGN.md)
- [Requirements traceability](REQUIREMENTS_TRACEABILITY.md)
- [Root-cause reports](ROOT_CAUSE_REPORTS.md)
- [Verification evidence](../evidence/VERIFICATION.md)
- [CTest configuration](../CMakeLists.txt)
