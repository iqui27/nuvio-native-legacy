#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
mkdir -p "$TMPDIR"
dir=$(mktemp -d "$TMPDIR/nuvio-plx-prefetch.XXXXXX")
trap 'rm -rf "$dir"' EXIT
export NUVIO_DADOS="$dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
renomes=()
for nome in tocar parar pausar buscar pausa_confirmada tocando pronto ativo falhou conflito_recurso pos duracao url_atual; do
  renomes+=("-Dvideo_$nome=nv_base_video_$nome")
done
cc "${flags[@]}" "${renomes[@]}" -c src/video.c -o "$dir/video.o"
cc "${flags[@]}" -Dmkvass_prebuscar=nv_base_mkvass_prebuscar \
  -Dmkvass_prebusca_fase=nv_base_mkvass_prebusca_fase -c src/mkvass.c -o "$dir/mkvass.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/video.c|src/mkvass.c) continue;; esac
  sources+=("$source")
done
cc "${flags[@]}" -DNV_SHOT_HOOKS "${sources[@]}" tests/plx_legenda_prefetch.c "$dir/video.o" "$dir/mkvass.o" \
  -o "$dir/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$dir/test"
