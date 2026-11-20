#!/bin/bash
# Approved pt-BR wizard proof, production renderer and offline navigation.
set -euo pipefail
cd "$(dirname "$0")/.."
output=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/round4
binary=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/wizard-shot-final
reuse=1
if [ ! -x "$binary" ] || [ src/srvsetup.c -nt "$binary" ] || [ tests/servidores_setup_shot.c -nt "$binary" ]; then reuse=0; fi
for scale in ${1:-1 .85}; do
  for glass in ${2:-1 0}; do
    NUVIO_SHOT_WIZARD=1 NUVIO_SHOT_QUICK=1 NUVIO_SHOT_TRIOS=1 \
      NUVIO_SHOT_ESCALA_TEXTO=$scale NUVIO_SHOT_VIDRO=$glass \
      NUVIO_SHOT_BIN="$binary" NUVIO_SHOT_REUSE_BIN=${reuse:-0} \
      bash tests/servidores_setup_shot.sh "$output/$scale-$glass"
    reuse=1
  done
done
