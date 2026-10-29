#!/bin/bash
# Focused rendered regression: dated upcoming episodes at 100% and 130%.
set -euo pipefail
cd "$(dirname "$0")/.."
export NV_ANCHOR_UPCOMING=1
export NV_ANCHOR_SHOTS="${NV_ANCHOR_SHOTS:-/Volumes/ExternalSSD/tmp/rev/escalafixa-upcoming}"
bash tests/escala_ancoras.sh
