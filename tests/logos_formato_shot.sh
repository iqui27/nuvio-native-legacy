#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Captura dos selos de formato do player e da tabela de amostras (ver o .c).
# Nao entra na suite: precisa de janela GL e de olho humano.
#
#   bash tests/logos_formato_shot.sh /tmp/nv-logos-shots/depois/player
set -eu
cd "$(dirname "$0")/.."
FLAGS="-O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations -Wno-macro-redefined"
OBJ=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-logos-obj.XXXXXX)
trap 'rm -rf "$OBJ"' EXIT
# video.c com os leitores de formato e de estado renomeados: o harness define os dele.
cc -c $FLAGS -Dvideo_largura=video_largura_mac -Dvideo_hdr=video_hdr_mac \
  -Dvideo_tocar=video_tocar_mac -Dvideo_ativo=video_ativo_mac -Dvideo_pronto=video_pronto_mac -Dvideo_tocando=video_tocando_mac -Dvideo_tem_atmos=video_tem_atmos_mac -Dvideo_tem_dolby_vision=video_tem_dv_mac \
  src/video.c -o "$OBJ/video.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/video.c) continue;; esac
  sources+=("$source")
done
NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-logos-dados.XXXXXX)
export NUVIO_DADOS
cc $FLAGS "${sources[@]}" "$OBJ/video.o" tests/logos_formato_shot.c -o ${TMPDIR:-/tmp}/nuvio-logos-shot \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
${TMPDIR:-/tmp}/nuvio-logos-shot "$@"
rm -rf "$NUVIO_DADOS"
