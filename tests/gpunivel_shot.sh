#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A home nos niveis de GPU 0, 1 e 2 (src/gpunivel.h), em BMP, com o
# preenchimento por modo e o tempo de GPU do Mac. Nao entra na suite
# (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL, de REDE
# (metahub) e de olho humano. Ver tests/gpunivel_shot.c.
#
#   bash tests/gpunivel_shot.sh /tmp/nuvio-gpunivel
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-gpunivel-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

ENV_D=$(tools/env.sh --allow-unconfigured 2>/dev/null || true)
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
eval cc '"${sources[@]}"' tests/gpunivel_shot.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-gpunivel-shot \
  -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-gpunivel-shot "$NUVIO_DADOS" "$@"
