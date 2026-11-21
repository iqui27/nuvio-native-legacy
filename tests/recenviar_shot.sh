#!/bin/bash
export TMPDIR="${TMPDIR:-/mnt/block-volume}"
TMPDIR="${TMPDIR%/}"
# Fixture captures of the "Recommend to a friend" sheet (tests/recenviar_shot.c).
# Usage: tests/recenviar_shot.sh <output-prefix>
#   NUVIO_SHOT_IDIOMA=6   German (any IDIOMA_* index)
#   NUVIO_SHOT_SOLIDO=1   Glass off
#   RE_FONTE=/abs/recenviar.c  photograph another version of the module
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d "${TMPDIR:-/mnt/block-volume}/nuvio-send-shot.XXXXXX")
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recenviar.c) continue;; esac
  sources+=("$source")
done
fonte=()
if [ -n "${RE_FONTE:-}" ]; then fonte=(-DRE_FONTE="\"$RE_FONTE\""); fi
# AJUSTES_TESTE exposes ajustes_teste_*; NV_REC_URL makes recomenda_ativo() true
# so the "-amigosPerfis" option is visible to the settings-navigation fixture.
flags=(-DAJUSTES_TESTE '-DNV_REC_URL="https://fixture.invalid"' -O1 -Isrc -Wno-deprecated-declarations)
CC="${CC:-clang}"
if [ "$(uname)" = Darwin ]; then
  "$CC" "${sources[@]}" tests/recenviar_shot.c ${fonte[@]+"${fonte[@]}"} "${flags[@]}" -o "$NUVIO_DADOS/shot" \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-macro-redefined
else
  read -r -a sdk <<< "$(pkg-config --cflags sdl2 SDL2_image SDL2_ttf)"
  read -r -a libs <<< "$(pkg-config --libs sdl2 SDL2_image SDL2_ttf)"
  "$CC" "${sources[@]}" tests/recenviar_shot.c ${fonte[@]+"${fonte[@]}"} "${flags[@]}" "${sdk[@]}" \
    -DNV_LINUX_DESKTOP -DGL_UNPACK_ROW_LENGTH=0x0CF2 -o "$NUVIO_DADOS/shot" \
    "${libs[@]}" -lz -lGLESv2 -pthread -lm -ldl
fi
"$NUVIO_DADOS/shot" "$@"
