#!/bin/bash
# Fundo pelo caminho da luz imersiva (Frost / Arte borrada): o quadro pequeno recebe a pintura e a tela
# assada bate com o desenho direto. Ver tests/fundo_assado.c.
#
#   bash tests/fundo_assado.sh        # GL 2.1 do Mac
#   bash tests/fundo_assado.sh gles   # GLES2 do ANGLE (precisa do emulador do Android SDK e do NDK)
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-fundo-assado.XXXXXX")
trap 'rm -rf "$work"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" != src/main.c ] && sources+=("$source")
done
gl=(-framework OpenGL)
if [ "${1:-}" = gles ]; then
  angle="${NV_ANGLE_DIR:-$HOME/Library/Android/sdk/emulator/lib64/gles_angle}"
  ndk=$(ls -d "$HOME"/Library/Android/sdk/ndk/*/toolchains/llvm/prebuilt/*/sysroot/usr/include 2>/dev/null | tail -1)
  [ -f "$angle/libGLESv2.dylib" ] && [ -n "$ndk" ] || { echo "sem ANGLE/NDK: pulando o modo gles"; exit 0; }
  mkdir -p "$work/inc"
  for d in GLES2 EGL KHR; do ln -s "$ndk/$d" "$work/inc/$d"; done
  gl=(-DNV_GLES_NO_MAC -I"$work/inc" -L"$angle" -lEGL -lGLESv2 -Wl,-rpath,"$angle" -Wl,-headerpad_max_install_names)
fi
cc "${sources[@]}" tests/fundo_assado.c -Isrc -o "$work/test" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz "${gl[@]}" \
  -Wno-deprecated-declarations -Wno-macro-redefined
if [ "${1:-}" = gles ]; then   # o ANGLE do emulador se chama ./libEGL.dylib
  for l in libEGL libGLESv2; do install_name_tool -change "./$l.dylib" "$angle/$l.dylib" "$work/test" || true; done
  codesign -f -s - "$work/test" 2>/dev/null || true
fi
mkdir -p "$work/dados"
NUVIO_DUMP_FUNDO_DIR="$work" NUVIO_DADOS="$work/dados" NUVIO_TESTE_DIR="$work/dados" "$work/test" 2>&1 |
  grep -vE "^\[(tex|arte|cat|desc|home|txt|corviva|ajustes|dados|perfil|badges)[a-z-]*\]"
exit "${PIPESTATUS[0]}"
