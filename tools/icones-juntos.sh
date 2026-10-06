#!/bin/bash
# Icones das REACOES do Watch Together (jt_*.png). Desenho NOSSO, no traco do
# Lucide (grade 24, traco 2, pontas redondas), para casar com os aj_* — e o
# mesmo desenho da pagina do celular (servidor/juntos/src/celular.js, ICONES):
# a fonte de verdade sao os SVG em deploy/app/art/icones/juntos/. Nada de
# emoji: as fontes das TVs nao tem emoji colorido.
# Rasteriza como tools/icones-lucide.sh: 128x128, branco, forma no alfa.
#   bash tools/icones-juntos.sh     (precisa de sips e magick)
set -e
cd "$(dirname "$0")/.."
DIR=deploy/app/art/icones
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
for f in "$DIR"/juntos/*.svg; do
  n=$(basename "$f" .svg)
  sed -e 's/currentColor/#ffffff/g' -e 's/width="24"/width="512"/' -e 's/height="24"/height="512"/' "$f" > "$TMP/a.svg"
  sips -s format png "$TMP/a.svg" --out "$TMP/a.png" >/dev/null
  magick "$TMP/a.png" -alpha extract -filter Box -resize 128x128 "$TMP/m.png"
  magick -size 128x128 xc:white "$TMP/m.png" -alpha off -compose CopyOpacity \
    -composite -strip -define png:color-type=6 "$DIR/jt_$n.png"
done
echo "icones-juntos: $(ls "$DIR"/juntos/*.svg | wc -l | tr -d ' ') icones em $DIR/jt_*.png"
