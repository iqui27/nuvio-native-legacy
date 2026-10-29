#!/bin/bash
# One native target: shared production objects, navigation and source-state checks.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-chipfonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$work/obj" "$work/data"
flags=(-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
export NV_CHIP_OBJECTS="$work/obj"
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/detail.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" | xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_CHIP_OBJECTS/$(basename "$0").o"' '{}' "${flags[@]}"
cc "${flags[@]}" -c src/streams.c -o "$work/streams.o"
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
cc "${flags[@]}" tests/chipfonte.c "$work/obj/"*.o "$work/streams.o" -o "$work/test" "${libs[@]}"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/test"
cc "${flags[@]}" -c src/detail.c -o "$work/detail.o"
cc "${flags[@]}" tests/chipfonte_status.c "$work/obj/"*.o "$work/detail.o" -o "$work/status" "${libs[@]}"
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" "$work/status"
out=/Volumes/ExternalSSD/tmp/rev/chipfonte
mkdir -p "$out"
cc "${flags[@]}" tests/detalhe_glass_shot.c "$work/obj/"*.o "$work/streams.o" -o "$work/shot" "${libs[@]}"
shotdata=$(mktemp -d "$work/nuvio-detglass-dados.XXXXXX")
NUVIO_DADOS="$shotdata" NUVIO_TESTE_DIR="$shotdata" NUVIO_CHIPFONTE_PROOF=1 \
  NUVIO_SHOT_IDIOMA=1 NUVIO_SHOT_TOPO=0 NUVIO_SHOT_SOURCE=1 \
  "$work/shot" "$out"
