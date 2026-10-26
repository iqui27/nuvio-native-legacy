#!/bin/bash
# Reuse identical native objects across J4's four offline render harnesses.
set -eu
cd "$(dirname "$0")/.."
source_test=$1
binary=$2
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-j4-objects.XXXXXX")
trap 'rm -rf "$work"' EXIT
export CCACHE_DIR="${TMPDIR:-/tmp}/nuvio-j4-ccache"
compiler=(cc)
if command -v ccache >/dev/null; then compiler=(ccache cc); fi
objects=()
pids=()
flags=()
if [ "$source_test" = tests/nav21.c ]; then flags+=('-DNV_REC_URL="http://127.0.0.1:8799"'); fi
for source in src/*.c src/dts/*.c "$source_test"; do
  case "$source" in src/main.c) continue;; esac
  case "$source_test:$source" in
    tests/nav21.c:src/app.c|tests/pessoas_shot.c:src/pessoas.c|tests/pessoas_shot.c:src/recomenda.c|tests/social_tela_shot.c:src/social.c|tests/filmografia_shot.c:src/filmodados.c) continue;;
  esac
  object="$work/$(basename "$source" .c).o"
  "${compiler[@]}" -c "$source" -o "$object" -DAJUSTES_TESTE -O1 -g -Isrc \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -Wno-deprecated-declarations -Wno-macro-redefined "${flags[@]}" &
  pids+=($!)
  objects+=("$object")
  if [ ${#pids[@]} -ge 4 ]; then wait "${pids[0]}"; pids=("${pids[@]:1}"); fi
done
for pid in "${pids[@]}"; do wait "$pid"; done
cc "${objects[@]}" -o "$binary" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
