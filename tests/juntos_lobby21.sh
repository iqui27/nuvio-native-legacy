#!/bin/bash
# Full-bleed Watch Together lobby (owner round 26/10): states at 100% glass and
# 85% solid, invite panel, shared Sources sheet, D-pad, plus a motion strip.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/juntoslobby
export NUVIO_JUNTOS_ART=/Volumes/ExternalSSD/tmp/rev/juntoscinema-nativo-png/v3/assets
mkdir -p "$out"
bash tests/juntos_shot.sh "$out/" --lobby21
for tag in 100-glass 85-solid; do
  magick "$out/$tag"-m*.png -resize 640x360 +append "$out/$tag-strip-motion.png"
  rm "$out/$tag"-m*.png
done
for image in "$out"/*.png; do
  magick "$image" -format '%[fx:standard_deviation>0.05]' info: | grep -qx 1
done
echo "juntos_lobby21: ok ($out)"
