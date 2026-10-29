# Reproducing the captured checks

Run from the labline folder. Logs in evidence were captured using these commands (the executor used an absolute path to its pip-installed cmake/ctest).

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build -V
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug --parallel 4
ctest --test-dir build-debug --output-on-failure
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DLABLINE_ASAN=ON
cmake --build build-asan --parallel 4
ctest --test-dir build-asan --output-on-failure
cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DLABLINE_TSAN=ON -DLABLINE_UNSAFE_RACE=ON
cmake --build build-tsan --parallel 4
ctest --test-dir build-tsan --output-on-failure
./build-tsan/unsafe_race_demo
bash scripts/run_all.sh
```

The unsafe-race command is expected to report a diagnostic and exit nonzero. Default ASan+UBSan did not pass in the supplied runtime; see VERIFICATION.md. Do not use its build success as a substitute for test success.

The minimal timed-mutex investigation was compiled with `g++ -std=c++17 -fsanitize=thread -g -pthread evidence/initial/timed_mutex_probe.cpp -o /tmp/labline-timed-probe`. It is retained as diagnostic evidence, not used in final production code or normal tests.
