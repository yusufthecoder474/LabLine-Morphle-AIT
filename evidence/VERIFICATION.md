
# Verification matrix

The original local Linux verification was executed on 29 October 2026. Compiler and platform details for that run are in [`environment.json`](environment.json). A separate Windows 11 / Ubuntu 24.04 WSL2 verification was completed on the same date and is documented below. Results are based on recorded execution, not inferred from code inspection.

| Configuration or experiment | Result | Evidence |
|---|---|---|
| Release configure/build (original Linux run) | PASS; no compiler warnings in captured log | `release_configure.log`, `release_build.log` |
| Release CTest (original Linux run) | PASS, 17/17 | `release_ctest.log`, `run_all.log` |
| Debug configure/build/CTest (original Linux run) | PASS, 17/17 | `debug_configure.log`, `debug_build.log`, `debug_ctest.log` |
| TSan configure/build/CTest, final normal suite (original Linux run) | PASS, 17/17; no reported race in exercised runs | `tsan_configure.log`, `tsan_build.log`, `tsan_ctest.log` |
| Optional intentionally racy target | Expected TSan data-race diagnostic captured | `unsafe_race.log` |
| ASan+UBSan build (original Linux run) | PASS | `asan_configure.log`, `asan_build.log` |
| ASan+UBSan default CTest (original Linux run) | ENVIRONMENT BLOCKED: 17 failed test processes due to LeakSanitizer runtime/process-inspection errors; **not sanitizer-clean** | `asan_ctest.log` |
| Serial/concurrent experiment (original Linux run) | PASS, 18 matched runs: job counts 10/50/100, three seeds, two worker settings | `measured/results.csv`, `REPORT.md` |
| Fault/boundary experiment (original Linux run) | PASS, five runs: ack/crc/slow/crash/timeout; assertions executed | `measured/` fault folders, `run_all.log` |
| Audit negative controls (original Linux run) | PASS, six alterations detected; original records preserved | `negative-controls/*.json` |
| `run_all.sh` (original Linux run) | PASS, including build, tests and experiment runner | `run_all.log` |
| Fresh ZIP extraction build and CTest (original Linux run) | PASS, 17/17 | `package_configure.log`, `package_build.log`, `package_ctest.log` |
| Windows 11 / Ubuntu 24.04 WSL2 launch and run | **PASS**: local CMake build, 17/17 CTest, 18 matched runs and five fault runs | [`WSL2_REPORT_2026-10-29.md`](WSL2_REPORT_2026-10-29.md); WSL2 verification section below |
| Hosted GitHub Actions — run #2 | **PASS**: Release 17/17, ASan 17/17, TSan 17/17; Release Python experiment runner completed | [Successful GitHub Actions run #2](https://github.com/yusufthecoder474/LabLine-Morphle-AIT/actions/runs/40851037411) |
| Physical hardware integration | NOT IMPLEMENTED | Software simulation only |

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

CTest counts test cases, not every assertion inside them. There is no claim of exhaustive concurrency verification. Unsafe race diagnostics are kept separate from the passing normal TSan suite.

## Retained initial results

`initial/` contains the first timed-mutex TSan failures and minimal reproduction, plus a preliminary experiment made before the lock portability adjustment. Those preliminary timings overlapped toolchain work and are not used in the final comparison. `measured/` contains the final source's experiment. Its original output directory was renamed to `measured` after completion; captured command metadata retains the actual original paths.

No sanitizer suppressions or leak-detection workarounds were used in the original run. The simulator child exits via `_exit` after its function returns; child heap leak checking is not claimed even on a platform where parent LeakSanitizer works.

## Windows 11 / Ubuntu 24.04 WSL2 verification — 29 October 2026

- **Platform:** Windows 11 host, Ubuntu 24.04 under WSL2.
- **Toolchain:** GCC 13.3.0, Python 3.12.3.
- **CMake build:** PASS.
- **CTest:** 17/17 passed, 0 failed; total CTest time 3.84 seconds.
- **Matched no-fault experiments:** 18 runs; 960/960 jobs completed.
- **Fault experiments:** Five scenarios executed with the expected outcomes (lost ACK, bad CRC, slow device, simulated crash, timeout). The crash and timeout scenarios intentionally include failed/unknown jobs.
- **Audit consistency:** Zero anomalies across 23 measured runs, as reported by the experiment checker.
- **100-job mean throughput:** 33.07 jobs/s with one worker versus 65.69 jobs/s with four workers, averaged over three paired trials.
- **100-job mean p95 job latency:** 312.25 ms with one worker versus 245.35 ms with four workers.
- **Captured report:** [`WSL2_REPORT_2026-10-29.md`](WSL2_REPORT_2026-10-29.md).

These WSL2 measurements are distinct from the earlier Linux run; timings can differ across hosts and scheduling conditions. The WSL2 run does **not** establish new local ASan/UBSan/TSan results.

## Hosted GitHub Actions verification — 29 October 2026

[GitHub Actions run #2](https://github.com/yusufthecoder474/LabLine-Morphle-AIT/actions/runs/40851037411) completed successfully on Ubuntu 24.04. The Release, ASan and TSan build/test jobs each passed 17/17 CTest cases, and the Release Python experiment runner completed successfully. These are hosted results for the workflow's checked-in configurations and are separate from the earlier local LeakSanitizer-blocked attempt.

The original local ASan+UBSan runtime limitation remains documented above. No physical hardware integration, clinical validation, or hard real-time guarantee is claimed.
