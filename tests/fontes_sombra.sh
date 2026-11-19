#!/bin/bash
# Sources sheet: no drop shadow, tab change slides the pill. Not in the suite (GL window).
#   bash tests/fontes_sombra.sh <dir> [git-ref-of-old-streams.c]
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/fontessombra/after}"
mkdir -p "$out" "$D/data"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  if [ "$source" = src/streams.c ] && [ -n "${2:-}" ]; then
    git show "$2:src/streams.c" > "$D/streams.c"
    source="$D/streams.c"
  fi
  sources+=("$source")
done
bin="$D/test"
flags=(-DNV_SHOT_HOOKS -Isrc -O0 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined)
if [ -n "${2:-}" ] && ! rg -q 'stream_folha_pilula_x' "$D/streams.c"; then
  flags+=(-DNV_BASELINE)
fi
objs=()
for source in "${sources[@]}" tests/fontes_sombra.c; do objs+=("$D/$(echo "$source" | tr '/' '_').o"); done
printf '%s\n' "${sources[@]}" tests/fontes_sombra.c |
  xargs -P 8 -I{} sh -c 'cc "$@" -c {} -o "$0/$(echo {} | tr / _).o"' "$D" "${flags[@]}"
cc "${objs[@]}" -o "$bin" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
modes=(no-clock detail player detail-glass player-glass)
for mode in "${modes[@]}"; do
  NUVIO_DADOS="$D/data" NUVIO_TESTE_DIR="$D/data" "$bin" "$out/$mode" "$mode"
done
