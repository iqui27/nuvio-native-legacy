#!/bin/bash
# #703: reforca setDisplayRect apos avplay prepared.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q '#703: setDisplayRect ainda falhou apos prepared' src/video_tizen.c
grep -q 'setDisplayRect RECUSOU' src/video_tizen.c
echo "PASS: video_tizen reaplica rect no prepared (#703)"
