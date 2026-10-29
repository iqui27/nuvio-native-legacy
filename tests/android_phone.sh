#!/bin/bash
# Phone compatibility gate: shared APK, landscape/safe bounds, Back and touch.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
python3 tests/android_interface_mode.py
bash tests/ajustes_ux_toque_modo.sh
bash tests/teclado_ime_modo.sh
