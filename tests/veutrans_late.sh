#!/bin/bash
# Recheck late textures without repeating the already captured zoom frames.
set -eu
cd "$(dirname "$0")/.."
export NV_VEU_LATE_ONLY=1
export NV_BINARY="${TMPDIR:-/Volumes/ExternalSSD/tmp}/nuvio-veutrans-late-final-shot"
bash tests/veutrans.sh
