#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
stamp="$(date -u +%Y%m%dT%H%M%SZ)-$$"
python3 python/run_experiments.py --binary build/labline --output "evidence/run-$stamp"
printf '\nReport: evidence/run-%s/REPORT.md\n' "$stamp"
