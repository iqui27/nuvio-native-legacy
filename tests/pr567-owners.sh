#!/bin/bash
# Focused P1 credential isolation and P2 idle modal regressions; no network/device.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/pr567-modal.py
bash tests/tpk-cdn.sh
