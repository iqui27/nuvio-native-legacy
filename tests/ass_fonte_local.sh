#!/bin/bash
# Requires real libass and SDL_ttf. Missing dependencies are failures, not skips.
set -eu
cd "$(dirname "$0")/.."
pkg-config --exists libass fribidi sdl2 SDL2_ttf
font="${NV_TEST_FONT:-deploy/app/fonts/JetBrainsMonoNL-Regular.ttf}"
[ -f "$font" ] || { echo 'ass_fonte_local: NV_TEST_FONT must name a real TrueType fixture' >&2; exit 1; }
bold="${NV_TEST_FONT_BOLD:-deploy/app/fonts/JetBrainsMonoNL-SemiBold.ttf}"
[ -f "$bold" ] || { echo 'ass_fonte_local: NV_TEST_FONT_BOLD must name the bold fixture' >&2; exit 1; }
arabic="${NV_TEST_FONT_ARABIC:-deploy/app/fonts/NotoNaskhArabic-Regular.ttf}"
[ -f "$arabic" ] || { echo 'ass_fonte_local: NV_TEST_FONT_ARABIC must name the attached fallback fixture' >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ass-font.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_DEFAULT_SOURCE -DNV_ASS_LIBASS -Isrc -g -Wall -Wextra)
platform=(-lm -pthread)
if [ "$(uname -s)" = Darwin ]; then platform+=(-framework OpenGL); else platform+=(-lGLESv2); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Linux ]; then flags+=(-fno-pie -no-pie); fi
fi
# assrender.c is included verbatim by the fixture so internal cache invariants can be checked.
# shellcheck disable=SC2046
"${CC:-cc}" "${flags[@]}" $(pkg-config --cflags libass fribidi sdl2 SDL2_ttf) \
  tests/ass_fonte_local.c src/legfontes.c src/bidi.c src/plainass.c \
  -o "$work/test" $(pkg-config --libs libass fribidi sdl2 SDL2_ttf) "${platform[@]}"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$work/test" "$font" "$bold" "$arabic"
python3 tests/font_fixture.py "$work/fixture.otf"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$work/test" "$work/fixture.otf" "$bold" "$arabic"
