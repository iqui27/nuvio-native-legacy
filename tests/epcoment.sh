#!/bin/bash
# Real detail/menu UI, deterministic Trakt responses; no network.
set -euo pipefail
cd "$(dirname "$0")/.."
export NUVIO_DADOS
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-epcoment.XXXXXX")
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/detail.c|src/extras.c|src/epcoment.c|src/notasui.c) continue;; esac
  sources+=("$source")
done
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
cc "${flags[@]}" -Drede_pedir=epcoment_pedir -Drede_baixar_com=epcoment_baixar -Dtrakt_cabecalhos=epcoment_cabecalhos -Dtrakt_cabecalhos_publicos=epcoment_publicos -c src/extras.c -o "$NUVIO_DADOS/extras.o"
cc "${flags[@]}" -Dtxt_linha=epreview_texto -Dgfx_cor=epreview_color -c src/epcoment.c -o "$NUVIO_DADOS/epcoment.o"
cc "${flags[@]}" -Dbadge_imdb=epreview_badge_imdb -c src/notasui.c -o "$NUVIO_DADOS/notasui.o"
cc "${flags[@]}" -DNV_TEST_EPCOMENT "${sources[@]}" "$NUVIO_DADOS/extras.o" "$NUVIO_DADOS/epcoment.o" "$NUVIO_DADOS/notasui.o" tests/menuep.c -o "$NUVIO_DADOS/teste" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
mkdir -p /Volumes/ExternalSSD/tmp/rev/eppolish
"$NUVIO_DADOS/teste" "${1:-/Volumes/ExternalSSD/tmp/rev/eppolish/after}"
