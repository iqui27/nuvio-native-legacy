#!/bin/bash
# pt-BR mock | reference | app proof for the three independent-review blockers.
set -euo pipefail
cd "$(dirname "$0")/.."
output=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/round5
binary=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/review-shot
reuse=0
for scale in 1 .85; do
  for glass in 1 0; do
    NUVIO_SHOT_WIZARD=1 NUVIO_SHOT_QUICK=1 NUVIO_SHOT_TRIOS=1 \
      NUVIO_SHOT_SCREEN=setup-emby-credentials,setup-plex-servidor,setup-manual,setup-jellyfin-senha \
      NUVIO_SHOT_ESCALA_TEXTO=$scale NUVIO_SHOT_VIDRO=$glass \
      NUVIO_SHOT_BIN="$binary" NUVIO_SHOT_REUSE_BIN=$reuse \
      bash tests/servidores_setup_shot.sh "$output/$scale-$glass"
    reuse=1
  done
done
