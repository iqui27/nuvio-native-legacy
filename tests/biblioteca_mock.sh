#!/bin/bash
# Approved Library visual states and D-pad assertions. Offline, temporary profile only.
set -euo pipefail
cd "$(dirname "$0")/.."
export NV_MODO=mock
bash tests/biblioteca_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/biblio-mock-png/final-app}"
