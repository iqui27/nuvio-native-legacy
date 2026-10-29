#!/bin/bash
# Review regressions plus native transition proof; no TV or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/juntos_fluxo.sh
review=/Volumes/ExternalSSD/tmp/rev/21-juntosfluxo-p1
mkdir -p "$review"
bash tests/juntos_shot.sh "$review/after" --transitions
shot_data=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntos-flow-shot.XXXXXX")
trap 'rm -rf "$shot_data"' EXIT
NUVIO_DADOS="$shot_data" "${TMPDIR:-/tmp}/nuvio-juntos-shot" "$review/flow" --flow
