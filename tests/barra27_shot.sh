#!/bin/bash
# A27 top chrome (avatar chip + top bar + clock island) at rest over the art, at
# "Opacidade do vidro" 60 / 78 / 92 and with reduced transparency (fosco off ->
# opaque fallback), each beside the matching glassdesign mockup when present.
# Needs a GL window; not part of the suite.
#
#   bash tests/barra27_shot.sh [outdir] [mockdir]
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/barra27}"
mock="${2:-/Volumes/ExternalSSD/tmp/rev/glassdesign}"
mkdir -p "$out"
NUVIO_SHOT_TOPO=1 NUVIO_SHOT_PILULA=1 NUVIO_SHOT_SO_MENU=1 NUVIO_SHOT_VIDRO_OPAC=78 \
  bash tests/vidro_menu_shot.sh "$out/build" >/dev/null
bin="${TMPDIR:-/tmp}"; bin="${bin%/}/nuvio-vidro-menu-shot"
run() {   # name opac fosco mockup
  dados=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-barra27.XXXXXX")
  NUVIO_DADOS="$dados" NUVIO_SHOT_TOPO=1 NUVIO_SHOT_PILULA=1 NUVIO_SHOT_SO_MENU=1 \
    NUVIO_SHOT_VIDRO_OPAC="$2" NUVIO_SHOT_VIDRO_FOSCO="$3" "$bin" "$out/$1" | grep custo: | head -1
  rm -rf "$dados"
  magick "$out/$1-rail.bmp" "$out/$1.png"
  magick "$out/$1.png" -crop 1920x140+0+0 +repage "$out/$1-topo.png"
  if [ -f "$mock/$4" ]; then
    magick "$out/$1-topo.png" \( "$mock/$4" -resize 1920x -crop 1920x140+0+0 +repage \) -append "$out/$1-lado.png"
  fi
  rm -f "$out/$1"-*.bmp
}
run opac60 60 1 mockup-A27-claro.png
run opac78 78 1 mockup-A27.png
run opac92 92 1 mockup-A27-tinto.png
run reduzida 78 0 mockup-A27-sem-transparencia.png
rm -f "$out"/build-*.bmp
ls "$out"/*.png
