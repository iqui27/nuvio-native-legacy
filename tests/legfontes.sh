#!/bin/bash
# Persistent private imports; actual SDL_ttf/FreeType, no graphics or document provider.
set -eu
cd "$(dirname "$0")/.."
font="${NV_TEST_FONT:-deploy/app/fonts/InterDisplay-Regular.ttf}"
if [ ! -f "$font" ]; then
  for candidate in /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf /System/Library/Fonts/Supplemental/Arial.ttf; do
    if [ -f "$candidate" ]; then font="$candidate"; break; fi
  done
fi
[ -f "$font" ] || { echo 'legfontes: set NV_TEST_FONT to a real TTF/OTF' >&2; exit 1; }
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legfontes.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=c11 -D_DEFAULT_SOURCE -DNV_ANDROID -Isrc -g -Wall -Wextra)
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Linux ]; then flags+=(-fno-pie -no-pie); fi
fi
# shellcheck disable=SC2046
"${CC:-cc}" "${flags[@]}" $(pkg-config --cflags sdl2 SDL2_ttf) tests/legfontes.c src/assrender.c \
  -o "$work/test" $(pkg-config --libs sdl2 SDL2_ttf)
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$work/test" "$font"
python3 tests/font_fixture.py "$work/fixture.otf"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$work/test" "$work/fixture.otf"
