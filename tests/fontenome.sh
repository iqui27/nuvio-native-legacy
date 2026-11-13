#!/bin/bash
# #735: offline focused-name regression, PNGs and weak-TV GL proof.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/fontenome
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-fontenome.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir -p "$out" "$work/data" "$work/obj"
export NV_FONTENOME_OBJECTS="$work/obj"
flags=(-DNV_SHOT_HOOKS -Isrc -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
sources=()
for source in src/*.c src/dts/*.c; do
  [[ "$source" != src/main.c && "$source" != src/streams.c ]] && sources+=("$source")
done
printf '%s\n' "${sources[@]}" |
  xargs -P 8 -I{} sh -c 'cc "$@" -c "$0" -o "$NV_FONTENOME_OBJECTS/$(basename "$0").o"' '{}' "${flags[@]}"
cc tests/fontenome.c "${flags[@]}" -DNV_FLUIDEZ_PERF "$work/obj/"*.o -o "$work/test" -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
# Bounded like fontes_lote_e.sh: an SDL startup hang dies by SIGALRM.
bounded() { perl -e 'alarm shift; exec @ARGV' 300 "$@"; }
NUVIO_DADOS="$work/data" NUVIO_TESTE_DIR="$work/data" bounded "$work/test" "$out"
# Existing source-sheet checks share the same objects; no repeated full builds.
for test in fontes_quadro fontes_lista fontes_formatter; do
  objects=("$work/obj/"*.o)
  extra=()
  if [[ $test == fontes_lista ]]; then
    cc src/streams.c "${flags[@]}" -c -o "$work/streams.o"
    objects+=("$work/streams.o")
  fi
  [[ $test == fontes_quadro ]] && extra+=(-finstrument-functions)
  cc "tests/$test.c" "${flags[@]}" ${extra[@]+"${extra[@]}"} "${objects[@]}" -o "$work/$test" \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
  mkdir -p "$work/$test-data"
  NUVIO_DADOS="$work/$test-data" NUVIO_TESTE_DIR="$work/$test-data" bounded "$work/$test"
done
