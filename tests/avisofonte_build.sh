#!/bin/bash
# Host compile only; never starts the app or touches user data.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
mkdir -p "$TMPDIR"
export NUVIO_MAC_BIN="$TMPDIR/nuvio-avisofonte-build"
export NUVIO_MAC_SO_COMPILAR=1
bash tools/mac.sh
