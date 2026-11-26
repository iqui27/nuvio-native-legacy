#!/bin/bash
# Texto simples (asspeso) x faixa/lote novo: ver tests/legenda_peso_corrida.c.
# Precisa do libass; sem ele, pula.
#   bash tests/legenda_peso_corrida.sh   (SANITIZE=1 com ASan/UBSan)
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass sdl2 fribidi; then
  echo "legenda_peso_corrida: libass ausente (teste ignorado)"; exit 0
fi
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-peso-corrida.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -O1 -g "${flags[@]}" -DNV_ASS_LIBASS -DNV_TEST_PLAINASS_LINKED -include tests/ass_pisca_gl.h -Isrc \
  tests/legenda_peso_corrida.c src/legenda.c src/plainass.c src/assrender.c src/assperf.c src/asspeso.c src/assfios.c \
  tests/ass_render_stub.c src/bidi.c \
  $(pkg-config --cflags --libs libass sdl2 fribidi) -pthread -lm \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$work/test"
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" "$work/test" 2>/dev/null
