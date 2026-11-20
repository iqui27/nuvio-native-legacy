#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
export NV_HOME_VAZIA=1
bash tests/homelayouts_shot.sh "$TMPDIR/homeqr-205"
