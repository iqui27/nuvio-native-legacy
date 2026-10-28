#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_TRAILS_ONLY=1
bash tests/conquistas_shot.sh /Volumes/ExternalSSD/tmp/rev/trilhas/trails
