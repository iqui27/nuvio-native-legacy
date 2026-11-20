#!/bin/bash
# Sources sheet on the detail page grows out of the clock island. Not in the suite (GL window).
#   bash tests/fontes_relogio_shot.sh <dir>
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
out="${1:-/Volumes/ExternalSSD/tmp/fontesrelogio/after}"
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
cc -DNV_SHOT_HOOKS "${sources[@]}" tests/fontes_relogio_shot.c -Isrc -o "$bin" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
modes=(detail home no-clock player)
[ -z "${2:-}" ] || modes=(detail)
for mode in "${modes[@]}"; do
  NUVIO_DADOS="$D/data" NUVIO_TESTE_DIR="$D/data" "$bin" "$out/$mode" "$mode"
done
