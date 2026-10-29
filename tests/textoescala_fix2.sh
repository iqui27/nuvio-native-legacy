#!/bin/bash
# Focused review regressions, including production draw-path geometry.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/textoescala_fix2.py
bash tests/textoescala_layout.sh
bash -n tests/textoescala_prova.sh
git diff --check
