#!/bin/bash
# Offline structural controls, native TV components, both materials and text sizes.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-playback-shot.XXXXXX")
trap 'rm -rf "$work"' EXIT
output=${1:-/Volumes/ExternalSSD/tmp/rev/j-21-srv-avancados-v2}
export NUVIO_SHOT_BIN="$work/shot" NUVIO_SHOT_REUSE_BIN=1 NUVIO_SHOT_QUICK=1
export NUVIO_SHOT_SCREEN="playback-plex playback-jellyfin playback-emby setup-bibliotecas setup-pronto"
for scale in 1 1.3; do
  for glass in 0 1; do
    NUVIO_SHOT_ESCALA_TEXTO=$scale NUVIO_SHOT_VIDRO=$glass \
      bash tests/servidores_setup_shot.sh "$output/$scale-$glass"
  done
done
