#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Queda da C9 no libass (ass_start_frame assert) ao reentregar fontes. Ver
# tests/ass_fontes.c. Precisa do libass do Homebrew; sem ele, pula.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_fontes: libass ausente (teste ignorado)"; exit 0
fi
flags=(-O1 -g)
platform=(-lm -pthread)
if [ "$(uname -s)" = Darwin ]; then platform+=(-framework OpenGL); else platform+=(-lGLESv2); fi
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
  if [ "$(uname -s)" = Linux ]; then flags+=(-fno-pie -no-pie); fi
fi
"${CC:-cc}" "${flags[@]}" -DNV_ASS_LIBASS -Isrc tests/ass_fontes.c src/assrender.c src/bidi.c src/plainass.c -o ${TMPDIR:-/tmp}/nuvio-ass-fontes-test \
  $(pkg-config --cflags --libs libass fribidi sdl2) "${platform[@]}" -Wno-deprecated-declarations -Wno-macro-redefined
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" ${TMPDIR:-/tmp}/nuvio-ass-fontes-test
