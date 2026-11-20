#!/bin/bash
# Focused persistence and full-pack regressions; no network or devices.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/fontevolta_persistir.sh
bash tests/partes.sh
python3 tests/pr567-review.py
