#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
if [[ ! -x build/labline ]]; then
  echo 'Building LabLine C++ executable (first run)...'
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel
fi
printf '\nOpen Chrome on Windows: http://localhost:8765\n'
printf 'Stop dashboard: Ctrl+C\n\n'
python3 web_demo/server.py
