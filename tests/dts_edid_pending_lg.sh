#!/bin/bash
# Host compile of the real webOS video.c EDID path. Stubs replace the player.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-edid-pending.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
export NUVIO_DADOS="$tmp/dados"
mkdir -p "$NUVIO_DADOS"
read -r -a sdl <<< "$(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf)"
cc -std=c11 -D_GNU_SOURCE -DNV_DTS_EDID_PENDING_TEST -O1 -g \
  -ffunction-sections -fdata-sections -Isrc -Wno-deprecated-declarations -Wno-macro-redefined \
  tests/dts_edid_pending_lg.c \
  src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c src/audioescolha.c \
  src/webosver.c src/cacheboost.c src/audioext.c \
  src/farolanel.c src/farolerro.c src/fontesonda.c \
  -Wl,-dead_strip -o "$tmp/test" "${sdl[@]}" -lz -framework OpenGL -ldl -pthread -lm
"$tmp/test"
