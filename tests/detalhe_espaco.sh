#!/bin/bash
# Title page "Row spacing" setting + B2 glass without bands
# (tests/detalhe_espaco_checks.h).
#   bash tests/detalhe_espaco.sh [out-dir]
set -o pipefail
cd "$(dirname "$0")/.."
out=${1:-$(mktemp -d "${TMPDIR:-/tmp}/nuvio-espaco.XXXXXX")}
NUVIO_DETAIL_CHECK=espaco NUVIO_SHOT_VIDRO=1 bash tests/detalhe_glass_shot.sh "$out" 2>/dev/null | grep -E '^(PASS|FAIL)'
