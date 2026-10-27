#!/bin/bash
# Focused guide preview regression, Android delayed-open path and native captures.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/guiapreview_android.py
bash tests/guiav5_bugs.sh
NUVIO_SHOT_SO='preview-idle-dark preview-playing preview-focus-moving' \
  bash tests/guiav5_principal_shot.sh /Volumes/ExternalSSD/tmp/rev/guiapreview
