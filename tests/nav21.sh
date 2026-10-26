#!/bin/bash
# Host navigation gate for the Settings tour. Physical remote proof is separate.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
bash tests/ajustes_tour.sh
