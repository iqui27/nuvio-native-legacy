#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-idle-ass.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" -DNV_ASS_LIBASS -include tests/ass_pisca_gl.h -Isrc \
  tests/legenda_idle_ass.c src/assrender.c tests/ass_render_stub.c src/bidi.c \
  $(pkg-config --cflags --libs libass sdl2) -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
# sdl2-compat finds SDL3 beside the executable under ASan on macOS.
sdl_libdir=$(pkg-config --variable=libdir sdl2)
[ ! -e "$sdl_libdir/libSDL3.dylib" ] || ln -s "$sdl_libdir/libSDL3.dylib" "$work/libSDL3.dylib"
"$work/test"
