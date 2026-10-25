#!/bin/bash
# Offline pt-BR comparison using the parent revision and the current renderer.
set -euo pipefail
cd "$(dirname "$0")/.."
saida="${1:-/Volumes/ExternalSSD/tmp/rev/issue656-genero-png}"
mkdir -p "$saida"
work="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-genre-shot.XXXXXX")"
trap 'rm -rf "$work"' EXIT
git show bb70a70ed:src/vertudo.c > "$work/vertudo-before.c"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/vertudo.c) continue;; esac
  sources+=("$source")
done
for state in before after; do
  flags=()
  if [ "$state" = before ]; then
    flags+=(-DNV_GENRE_BEFORE "-DNV_GENRE_VIEW_SOURCE=\"$work/vertudo-before.c\"")
  fi
  cc "${sources[@]}" tests/colecao_genero_shot.c -Isrc -Itests -o "$work/$state" -O1 \
    "${flags[@]}" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
  NUVIO_DADOS="$work" "$work/$state" "$saida"
done
