#!/bin/bash
# Watch Together in the film (owner 26/10): cinema-style start over the lobby,
# the session on the clock island, CH+ session sidebar (Saved sidebar shell).
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/juntoscinema
export NUVIO_JUNTOS_ART=/Volumes/ExternalSSD/tmp/rev/juntoscinema-nativo-png/v3/assets
mkdir -p "$out"
bash tests/juntos_shot.sh "$out/" --cinema
for image in "$out"/[1-5]-*.png; do
  magick "$image" -format '%[fx:standard_deviation>0.05]' info: | grep -qx 1
done
echo "juntos_cinema: ok ($out)"
