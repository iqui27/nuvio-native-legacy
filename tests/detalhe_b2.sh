#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export OBJ="${TMPDIR:-/tmp}/nuvio-detalheb2-obj"
NUVIO_DETAIL_CHECK=b2 NUVIO_SHOT_VIDRO=1 bash tests/detalhe_glass_shot.sh "${1:-/tmp/nuvio-detalhe-b2}"
