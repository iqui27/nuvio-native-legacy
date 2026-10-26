#!/bin/bash
# Host navigation gates for the Settings tour and personal-server setup.
# Physical remote proof is separate.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
bash tests/ajustes_tour.sh
export NUVIO_SHOT_FLOW_ONLY=1
exec bash tests/servidores_setup_fluxo.sh "$@"
