# Verification matrix

Executed locally on Linux, 29 October 2026. Compiler/platform details: `environment.json`. No results are inferred from code inspection.

| Configuration or experiment | Result | Evidence |
|---|---|---|
| Release configure/build | PASS, no compiler warnings in captured log | release_configure.log, release_build.log |
| Release CTest | PASS 17/17 | release_ctest.log; run_all.log |
| Debug configure/build/CTest | PASS 17/17 | debug_configure.log, debug_build.log, debug_ctest.log |
| TSan configure/build/CTest, final normal suite | PASS 17/17, no reported race in exercised runs | tsan_configure.log, tsan_build.log, tsan_ctest.log |
| Optional intentionally racy target | Expected TSan data-race diagnostic captured | unsafe_race.log |
| ASan+UBSan build | PASS | asan_configure.log, asan_build.log |
| ASan+UBSan default CTest | ENVIRONMENT BLOCKED: 17 failed test processes due to LeakSanitizer runtime/process-inspection errors; not sanitizer-clean | asan_ctest.log |
| Serial/concurrent experiment | PASS 18 matched runs, sizes 10/50/100, three seeds, two worker settings | measured/results.csv, REPORT.md |
| Fault/boundary experiment | PASS five runs: ack/crc/slow/crash/timeout, assertions executed | measured/ fault folders, run_all.log |
| Audit negative controls | PASS six alterations detected, original preserved | negative-controls/*.json |
| run_all.sh | PASS locally including build, tests and experiment runner | run_all.log |
| Fresh ZIP extraction build and CTest | PASS 17/17 | package_configure.log, package_build.log, package_ctest.log |
| WSL2 / Windows launch | NOT EXECUTED | README commands provided |
| Hosted GitHub Actions | NOT EXECUTED | workflow supplied, not published |
| Physical hardware integration | NOT IMPLEMENTED | simulation only |

## Exact test names

- `unit_roundtrip`: frame fields and standard CRC32 known vector.
- `unit_split_multiple`: every split boundary and two concatenated frames.
- `unit_crc`: corrupt payload rejected, subsequent valid frame decoded.
- `unit_length`: oversize stream header rejected; oversize encoder input throws.
- `unit_version`: wrong version rejected and parser recovers.
- `unit_state`: legal, illegal and terminal transitions.
- `unit_queue`: bounded producer/consumer, all 100 values and close behavior.
- `integration_concurrency`: ten complete jobs, 40 actions, real independent-device overlap, exclusive resources and arm/heater reservation.
- `integration_ack`: lost reply, retry and duplicate-free completion.
- `integration_crc`: corrupt reply counted, retry and duplicate-free completion.
- `integration_slow`: p95 response increase and additional soft deadline misses against baseline.
- `integration_crash`: disconnect resolves pending jobs and terminal accounting completes.
- `integration_timeout`: two retries, uncertain execution and run abort.
- `integration_repeat`: same seed produces the same completed job/operation set.
- `integration_audit`: clean record plus six altered record/summary cases.
- `integration_deadlock`: deterministic two-lock hang killed externally, then safe completion.
- `simulator_input_safety`: independent Python framing, bad CRC creates no action, device conflict NACK, in-flight/completed dedup and conflicting sequence reuse NACK.

CTest counts test cases, not every assertion inside them. No claim of exhaustive concurrency verification. Unsafe race diagnostics are kept separate from the passing normal TSan suite.

## Retained initial results

`initial/` contains the first timed-mutex TSan failures and minimal reproduction, plus a preliminary experiment made before the lock portability adjustment. Those preliminary timings overlapped toolchain work and are not used in the final comparison. `measured/` contains the final source's experiment. Its original output directory was renamed to `measured` after completion; captured command metadata retains the actual original paths.

No sanitizer suppressions or leak-detection workarounds were used. The simulator child exits via `_exit` after its function returns; child heap leak checking is not claimed even on a platform where parent LeakSanitizer works.
