#!/bin/bash
# Compile and run production artwork selection with the real poster provider.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/jobtmp}"
python3 tests/poster708.py "$@"
