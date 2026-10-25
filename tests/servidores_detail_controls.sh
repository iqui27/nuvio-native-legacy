#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=$(mktemp -d)
trap 'rm -rf "$output"' EXIT
export OBJ=/Volumes/ExternalSSD/tmp/rev/servidores-detail-obj
export NUVIO_SERVER_CHECK=1 NUVIO_SHOT_SERVIDORES=1 NUVIO_SHOT_IDIOMA=0
bash tests/detalhe_glass_shot.sh "$output" detalhe-chip
