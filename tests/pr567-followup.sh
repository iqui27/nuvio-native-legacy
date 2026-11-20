#!/bin/bash
# Focused P1 regressions: Next release ties and confirmed multipart rejection.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/pr567-review.py
bash tests/fontepref_lancamento.sh
