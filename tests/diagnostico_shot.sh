#!/bin/bash
# Capturas da tela de diagnostico em BMP (inicio, rodando, dois resultados e o
# cabecalho de Ajustes), sem rede e sem interacao.
#
#   bash tests/diagnostico_shot.sh /tmp/nuvio-diag
#   NUVIO_SHOT_PT=1 bash tests/diagnostico_shot.sh /tmp/nuvio-diag-pt
#
# Nao entra na suite (tools/testa-tudo.sh pula *_shot.sh): precisa de janela GL
# e de olho humano. NUVIO_DADOS e temporario: nada toca nos dados de quem roda.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-diag-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/diagnostico.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/diagnostico_shot.c -Isrc -o /tmp/nuvio-diagnostico-shot \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-diagnostico-shot "$@"
