#!/bin/bash
# Real Sources sheet, automatic selection and local Settings; offline only.
set -eu
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp
out=/Volumes/ExternalSSD/tmp/rev/fonteincompat
mkdir -p "$out"
if ! rg -q 'stream_definir_audio_capacidades' src/streams.c; then
  echo 'FAIL: Sources has no capability exclusions or session reveal'
  exit 1
fi
D=$(mktemp -d); trap 'rm -rf "$D"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/streams.c|src/ajustes.c) continue;; esac
  sources+=("$source")
done
flags=(-DNV_SHOT_HOOKS -Isrc -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined)
printf '%s\n' "${sources[@]}" tests/fonteincompat.c tests/fonteincompat_settings.c |
  xargs -P 8 -I{} sh -c 'cc "$@" -c {} -o "$0/$(echo {} | tr / _).o"' "$D" "${flags[@]}"
cc "$D"/*.o -o "$D/test" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
# Exercise the webOS list branch with a failing adapter, using the same objects.
cc "${flags[@]}" -DNV_WEBOS -c tests/fonteincompat.c -o "$D/tests_fonteincompat.c.o"
cc "$D"/*.o -o "$D/webos-probe" -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$D/webos-probe" unused
mkdir -p "$D/data"
NUVIO_DADOS="$D/data" NUVIO_TESTE_DIR="$D/data" "$D/test" "$out"
