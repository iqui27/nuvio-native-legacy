#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/android_live_ts.py
