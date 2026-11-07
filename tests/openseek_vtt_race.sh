#!/bin/bash
set -eu
export TMPDIR="${TMPDIR:-/tmp}"
cd "$(dirname "$0")/.."
case "$(uname -s)" in
  Darwin) strip=-Wl,-dead_strip; gl=(-framework OpenGL) ;;
  *) strip=-Wl,--gc-sections; gl=(-lGL) ;;
esac
cc -D_DEFAULT_SOURCE -ffunction-sections -fdata-sections -Isrc $(pkg-config --cflags sdl2 SDL2_image) \
  tests/openseek_vtt_race.c src/openseekproto.c src/osp1.c src/seekrvtt.c src/js.c src/redeurl.c \
  "$strip" $(pkg-config --libs sdl2 SDL2_image) "${gl[@]}" -o "$TMPDIR/nuvio-openseek-vtt-race"
"$TMPDIR/nuvio-openseek-vtt-race"
