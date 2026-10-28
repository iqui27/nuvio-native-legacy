#!/bin/bash
# Native hero navigation and PNG proof; no device access or deployment.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/detacoes
mkdir -p "$out"
bash tests/detail21.sh
bash tests/detail_acoes_gap.sh
bash tests/detail_eps.sh
export OBJ=/tmp/nv21-obj-$(basename "$PWD")
export NUVIO_DETAIL_SHOT=1 NUVIO_SHOT_IDIOMA=1 NUVIO_SHOT_TOPO=0
export NUVIO_SHOT_ESCALA=2 NUVIO_SHOT_FUNDO=0
NUVIO_SHOT_VIDRO=1 NUVIO_HERO_TOGETHER=1 bash tests/detalhe_glass_shot.sh "$out/glass" hero-actions
NUVIO_SHOT_VIDRO=0 bash tests/detalhe_glass_shot.sh "$out/solid" hero-actions
for name in collapsed expanded-rate morph-3 menu-open; do
  magick montage -font deploy/app/fonts/InterDisplay-Regular.ttf "$out/../detacoes-antes.jpg" "$out/glass/$name.png" \
    -geometry 960x540+0+0 -tile 2x1 "$out/$name-comparison.png"
done
magick montage -font deploy/app/fonts/InterDisplay-Regular.ttf "$out/glass/morph-1.png" "$out/glass/morph-2.png" \
  "$out/glass/morph-3.png" "$out/glass/morph-4.png" "$out/glass/morph-5.png" \
  -geometry 480x270+0+0 -tile 5x1 "$out/morph-strip.png"
echo 'PASS: comparison PNGs and five-frame morph strip'
