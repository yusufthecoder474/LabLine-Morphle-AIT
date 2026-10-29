# Morphle AIT keyword evidence

Alignment is based on the JD description supplied in the build prompt, not a newly retrieved job advertisement. VERIFIED means a named local check ran; SIMULATED identifies device scope. See `evidence/VERIFICATION.md` for sanitizer and platform limits.

| JD concept | Code | Test/evidence | Truthful status |
|---|---|---|---|
| C++ / Linux | All `src/*.cpp`, CMake | Release/Debug CTest logs; environment.json | VERIFIED locally |
| Integration with arm, pumps, heater, camera | simulator.cpp; controller.cpp | simulator_input_safety; measured simulator.csv | SIMULATED software subsystems; physical integration NOT IMPLEMENTED |
| Multiple threads | controller.cpp; transport.cpp | integration_concurrency, unit_queue | VERIFIED four workers + one communication thread |
| Processes / IPC | controller.cpp Child; transport.cpp | simulator_input_safety, integration_crash | VERIFIED Unix socketpair, fork before threads |
| Asynchronous operations | simulator.cpp scheduled completions; transport.cpp poll | max_overlapping_devices in results.csv | VERIFIED overlapping logical device operations |
| Mutex / synchronization | controller.cpp ordered locks; core.cpp queue and audit | integration_concurrency; TSan suite | VERIFIED exercised paths; no universal race-freedom proof |
| Deadlocks | tests/deadlock_demo.cpp | integration_deadlock | VERIFIED isolated hang externally terminated; safe ordering completes |
| Race conditions | optional tests/unsafe_race_demo.cpp | unsafe_race.log; normal tsan_ctest.log | VERIFIED diagnostic in unsafe target; normal suite passed |
| Timeouts / failure containment | transport.cpp fail_locked; Child destructor | integration_timeout, integration_crash | VERIFIED bounded retries and terminal accounting |
| Timing constraints | controller.cpp steady-clock metrics | results.csv deadline_misses | VERIFIED soft deadline measurement; hard real-time NOT IMPLEMENTED |
| Latency / jitter | controller.cpp percentile and variance calculation | REPORT.md and raw jobs.csv | VERIFIED measured general-purpose Linux observations |
| Functional tests | tests/unit.cpp | seven unit_* cases | VERIFIED |
| Integration / regression tests | tests/integration.py; device_protocol.py | nine integration_* plus simulator_input_safety | VERIFIED |
| Failure reproduction | --fault ack/crc/slow/crash | captured commands and logs; ROOT_CAUSE_REPORTS.md | VERIFIED injected synthetic cases |
| Logs / RCA / reliability | events.csv; audit_check.py | integration_audit; negative-controls | VERIFIED record checks and observed mitigations |
| Framed protocol / CRC / ACK | protocol.cpp; transport.cpp | unit roundtrip/split/crc/length/version, simulator_input_safety | VERIFIED software stream protocol; real UART NOT IMPLEMENTED |
| Sanitizers | CMake options | final TSan log; ASan log | TSan VERIFIED; ASan+UBSan runtime ENVIRONMENT BLOCKED by LeakSanitizer |
| GitHub Actions | .github/workflows/ci.yml | workflow source only | Hosted run UNVERIFIED / NOT EXECUTED |
| Windows WSL2 | README setup commands | no Windows host available | UNVERIFIED launch |
| Clinical safety / hardware validation | none | none | NOT IMPLEMENTED |
