#!/bin/bash
# Headless host regression: real guide snapshot, EPG arena replacement, text renderer.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-g5-epg-lifetime.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
sources=(src/epg.c src/xmltv.c src/ltv.c src/idcanal.c src/buscanorm.c src/guiav5.c src/js.c)
flags=(-O0 -g -Isrc -ffunction-sections -fdata-sections)
if [[ "$(uname -s)" == Darwin ]]; then
  flags+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -Wl,-dead_strip)
  libs=(-lz -lpthread -lm)
else
  read -r -a sdk_flags <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a sdk_libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  flags+=(-DNV_LINUX_DESKTOP -Wl,--gc-sections "${sdk_flags[@]}")
  libs=("${sdk_libs[@]}" -lz -lGLESv2 -pthread -lm -ldl)
fi
if [[ "${SANITIZE:-0}" == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${sources[@]}" tests/guiav5_epg_lifetime.c "${flags[@]}" "${libs[@]}" \
  -Wno-deprecated-declarations -o "$work/test"
"$work/test"
