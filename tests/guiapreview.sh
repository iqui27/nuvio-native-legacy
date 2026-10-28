#!/bin/bash
# Focused guide preview regression: Android delayed-open path, native captures and guide v5 preview checks.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/guiapreview_android.py
bash tests/video_android_live_window.sh
bash tests/guiav5_bugs.sh
NUVIO_SHOT_SO='preview-idle-dark preview-playing preview-focus-moving' \
  bash tests/guiav5_principal_shot.sh /Volumes/ExternalSSD/tmp/rev/guiapreview
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialib.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS" /Volumes/ExternalSSD/tmp/rev/guialib
bash tests/j4_compile.sh tests/guiapreview.c "$work/check"
"$work/check" /Volumes/ExternalSSD/tmp/rev/guialib
