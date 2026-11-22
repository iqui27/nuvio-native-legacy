#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Ajuste "Legenda ASS com estilo" no assrender.c de verdade. Ver tests/ass_peso.c.
# Precisa do libass >= 0.17 (o que assrender.c usa); sem ele, pula.
#   bash tests/ass_peso.sh
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --atleast-version=0.17 libass; then
  echo "ass_peso: libass >= 0.17 ausente (teste ignorado)"; exit 0
fi
case "$(uname -s)" in
  Darwin) GL="-I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib -lSDL2 -framework OpenGL" ;;
  *) GL="$(pkg-config --cflags --libs sdl2) -lGL" ;;
esac
DIR=$(mktemp -d "$TMPDIR/nuvio-ass-peso.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -O1 -DNV_ASS_LIBASS -include tests/ass_pisca_gl.h -Isrc tests/ass_peso.c src/assrender.c src/asspeso.c \
  tests/ass_render_stub.c -o "$DIR/test" $(pkg-config --cflags --libs --static libass) $GL \
  -pthread -lm -w
"$DIR/test" 2>/dev/null
