#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
# tools/testa-tudo.sh discovers this contract through tests/*.sh.
python3 tools/cache-contrato.py
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
obj=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cache-contract.XXXXXX")
trap 'rm -rf "$obj"' EXIT
export NV_CACHE_CONTRACT_FLAGS='-O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations'
printf '%s\n' "${sources[@]}" tests/cache_contrato.c | xargs -P 8 -I{} sh -c \
  'cc $NV_CACHE_CONTRACT_FLAGS -c "$1" -o "$2/$(echo "$1" | tr / _).o"' _ {} "$obj"
cc "$obj"/*.o -o "$obj/contract" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$obj/contract"
