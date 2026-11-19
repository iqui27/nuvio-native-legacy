#!/bin/bash
# Focused guide preview regression: Android delayed-open path, native captures and guide v5 preview checks.
# Artifact-path contract: callers redirect both output directories via
# GUIAPREVIEW_OUT (guide v5 captures) and GUIAPREVIEW_LIB_OUT (library check
# PNGs). With neither set, the original fixed paths below are used.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/guiapreview_android.py
bash tests/video_android_live_window.sh
bash tests/guiav5_bugs.sh
preview_out=${GUIAPREVIEW_OUT:-/Volumes/ExternalSSD/tmp/rev/guiapreview}
lib_out=${GUIAPREVIEW_LIB_OUT:-/Volumes/ExternalSSD/tmp/rev/guialib}
NUVIO_SHOT_SO='preview-idle-dark preview-playing preview-focus-moving' \
  bash tests/guiav5_principal_shot.sh "$preview_out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialib.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/data"
mkdir -p "$NUVIO_DADOS" "$lib_out"
bash tests/j4_compile.sh tests/guiapreview.c "$work/check"
"$work/check" "$lib_out"
