#!/bin/bash
# Comparable real-render PNGs and navigation evidence, no TV or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/detsecoes
export OBJ=/tmp/nv21-detsecoes-shot
export NUVIO_SHOT_VIDRO=1 NUVIO_DETSECOES_PROOF=1
bash tests/detalhe_glass_shot.sh "$out/after" lower-sections
for kind in movie series; do
  for section in comments quotes details studios; do
    magick "$out/before/$kind-$section.png" "$out/after/$kind-$section.png" +append "$out/$kind-$section-side-by-side.png"
  done
done
