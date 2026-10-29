#!/bin/bash
# Focused review regressions; no network or device required.
set -eu
cd "$(dirname "$0")/.."
bash tests/cdnlink.sh
python3 tests/pr567-session.py
python3 tests/pr567-review.py
python3 tests/pr567-modal.py
bash tests/fonteauto.sh
bash tests/tpk-cdn.sh
bash tests/partes.sh
bash tests/debrid.sh
