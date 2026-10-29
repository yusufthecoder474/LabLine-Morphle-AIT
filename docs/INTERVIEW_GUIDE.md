# Explain LabLine simply

## 60-second story

“LabLine is an AI-assisted C++ learning project about coordinating shared lab stations. Imagine two sample jobs both needing the same heater: the controller must reserve it, confirm the operation and keep a clear record. Four workers share five synthetic devices in a separate Linux process. Commands travel through a CRC-checked framed Unix socket stream. If a reply is lost, the retry reuses its sequence ID so the simulator does not repeat the action. If execution stays uncertain, the run stops issuing commands and records failed or unknown outcomes. A separate deadlock example shows opposite lock ordering hanging under an external timeout, while consistent ordering finishes. Across 18 local no-fault runs, all 960 jobs completed; at 100 jobs, mean throughput was 38.00 serial versus 74.44 jobs per second concurrent. These are software simulation results, not hardware or clinical claims.”

Use this only after reviewing and understanding the code. Describe your actual review, modifications and tests honestly; generated code is not proof of independent authorship.

## Five-minute walkthrough

**Minute 1 — problem:** Missing confirmation is ambiguous. A sample can be processed while its status record is lost. Show `ack` simulator COMPLETE, ACK_DROPPED and DEDUP beside controller RETRY and ACK. Explain that the lost item is the reply, not necessarily the operation.

**Minute 2 — workflow:** Open controller.cpp. Follow stain, heat, place, image. A worker owns one job's state. Either pump is acceptable. Arm plus heater are reserved together during placement; ascending resource ID order prevents the circular wait pattern used in the unsafe demo.

**Minute 3 — integration boundary:** Open protocol.cpp and transport.cpp. Explain START, version, length, sequence, device, operation, payload, CRC and END. A byte stream can split a frame anywhere. Only the communication thread touches socket send/receive. Workers wait on promises without holding the transport mutex.

**Minute 4 — fault diagnosis:** Show one log per fault. Corrupt reply is rejected; lost reply is retried; heater delay raises deadlines; child crash closes IPC. Unknown is counted within FAILED. No automatic restart, persistent deduplication or claim of exactly-once behavior across a crash.

**Minute 5 — evidence:** Show REPORT.md and VERIFICATION.md. 17 tests pass in Release/Debug/TSan. ASan+UBSan is blocked by LeakSanitizer in this environment. There are 18 no-fault runs and five fault/boundary runs. Explain p95, jitter, streaming admission and why three trials are useful but not a universal performance guarantee.

## Ten questions and defensible answers

1. **Why not one thread?** One thread is the baseline and is simpler. Multiple workers can overlap independent devices while respecting shared-station reservations. The recorded simulator intervals prove overlap rather than assuming it from thread count.
2. **What is a race condition here?** Uncoordinated access to shared mutable state can corrupt results or produce undefined behavior. The queue, pending map and logs have explicit ownership or mutexes. An isolated optional counter demo produces a real TSan race report; it is never linked into the controller.
3. **How does deadlock happen?** Worker A holds arm and waits for heater while B holds heater and waits for arm. The demo uses a rendezvous to establish both first locks. Killing that process is containment, not recovery of a locked mutex.
4. **Why use ordered locks?** All code reserves arm before heater. With no reverse path, that two-resource circular wait cannot form. RAII releases locks on ordinary scope exit; finite acquisition waits provide a failure outcome.
5. **What if a command executes but ACK disappears?** Retry the exact sequence. The simulator keeps in-progress and completed records, and returns a cached result without a second action. After two retries fail, abort further issuing and record uncertainty.
6. **Is that exactly once?** Only duplicate suppression within this bounded live simulator process. No persistent identity across restarts or atomic hardware transaction exists. Never call it distributed exactly-once delivery.
7. **Why a child process and socketpair?** It creates an actual software integration boundary with stream framing and independent failure. It demonstrates IPC without pretending to connect physical devices or implement UART.
8. **What are p95 and jitter?** Nearest-rank p95 is the observation at ceil(0.95*N) after sorting. Jitter here is the population standard deviation of mixed command response times. Neither is a hard real-time guarantee; small-sample p99 can simply equal the maximum.
9. **Does an audit anomaly prove an operation was missed?** No. It proves the record fails consistency checks. Missing timestamps can coexist with completed work; simulator action logs are additional evidence within this software model, not physical verification.
10. **What would you improve for real hardware?** First obtain device contracts and an approved test environment, then model device-specific acknowledgment, cancellation and safe states. Durable recovery, hardware interlocks, timing validation and domain review would be separate work, not features already delivered here.

## Three ATS-ready project bullets

Use only after personally reviewing/running the project; retain AI-assistance disclosure when discussing authorship.

- Developed an AI-assisted C++17 controller for five synthetic devices over software-only Linux IPC, completing 960/960 jobs across 18 matched serial/concurrent runs.
- Increased measured mean throughput from 38.00 to 74.44 jobs/s on 100-job synthetic workloads using four workers and ordered station locks across three paired trials.
- Validated retry deduplication, CRC rejection, delays and disconnect accounting through 17 tests passing in Release, Debug and TSan, with zero audit anomalies across 23 measured runs.

Do not claim physical robotics integration, real UART, hard real-time guarantees, hosted CI success, ASan/UBSan cleanliness or clinical benefits. The tests and results are local and bounded in scope.
