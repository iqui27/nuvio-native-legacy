#!/bin/bash
# Focused offline proof: overview, filter groups and long-list/detail views.
set -euo pipefail
cd "$(dirname "$0")/.."
export AIO_SHOT_DASHBOARD=1 AIO_SHOT_DETAILS=1 AIO_SHOT_STILLS=1 AIO_SHOT_PASSES=0-3
bash tests/aiostreams_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/aiodash/after}"
