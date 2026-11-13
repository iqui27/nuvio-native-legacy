#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vbo.XXXXXX")
trap 'rm -rf "$work"' EXIT
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
if [ "$(uname)" = Darwin ]; then gc=-Wl,-dead_strip; else gc=-Wl,--gc-sections; fi
cc -std=c11 -D_GNU_SOURCE -DNV_BUFFER_SEEK_TEST -O1 -g -ffunction-sections -fdata-sections -Isrc \
  tests/video_buffer_test_osd.c src/js.c src/linguas.c src/lsregistro.c src/audioinfo.c src/audioescolha.c src/webosver.c src/cacheboost.c \
  "$gc" -o "$work/test" "${sdl_flags[@]}" -ldl -pthread -lm
"$work/test"
# Compile the real renderer, RAM overlay and webOS backend with the release
# macros (no NV_HTTP_BUFFER_DIAG) and prove the player references them.
cc -std=gnu11 -D_GNU_SOURCE -DNV_WEBOS -DNV_BUFFER_SEEK_TEST -Werror=implicit-function-declaration -Isrc \
  "${sdl_flags[@]}" -c src/player.c -o "$work/player.o"
cc -std=gnu11 -D_GNU_SOURCE -DNV_WEBOS -DNV_BUFFER_SEEK_TEST -Werror=implicit-function-declaration -Isrc \
  "${sdl_flags[@]}" -c src/video.c -o "$work/video.o"
cc -std=gnu11 -D_GNU_SOURCE -DNV_WEBOS -Werror=implicit-function-declaration -Isrc \
  "${sdl_flags[@]}" -c src/ramoverlay.c -o "$work/ramoverlay.o"
nm "$work/ramoverlay.o" > "$work/ramoverlay.nm"
nm "$work/video.o" > "$work/video.nm"
nm "$work/player.o" > "$work/player.nm"
grep -q ' T _\{0,1\}ramoverlay_desenhar$' "$work/ramoverlay.nm"
grep -q ' T _\{0,1\}video_buffer_test_text$' "$work/video.nm"
for sym in ramoverlay_desenhar video_buffer_test_text ajustes_ram_verde ajustes_ram_overlay_alternar ajustes_buffer_dados; do
  grep -q " U _\{0,1\}$sym$" "$work/player.nm" || { echo "release player.o does not call $sym"; exit 1; }
done
echo "video_buffer_test_osd: release build wires buffer data, RAM overlay and GREEN"
