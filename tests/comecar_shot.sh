#!/bin/bash
# Capturas da tela "Começar" (2.1) em PNG 1920x1080: os 4 passos e o teclado
# da TV por cima. Antes, as regras por evento de tecla (teclado D-pad, gravacao
# so no fim, Stremio/AIOManager abrem o fluxo de importação, Voltar/Pular, upgrade com dados, pedidos
# do fim) e a varredura de navegação D-pad. Janela GL ESCONDIDA e desenho num FBO: nada
# aparece na tela de quem roda. Fica fora da suite (*_shot.sh): precisa de GL.
# Sem rede: traktauth_comecar parte só com a chave de rede, e aqui ele falha
# limpo — a regra testada é a de UI.
#
# Default: all steps at 80/85/100/115/130%, with control and painted bounds checks.
# Set NUVIO_SHOT_ESCALA_TEXTO to capture a single scale instead.
#   bash tests/comecar_shot.sh /tmp/start
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-comecar-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c|*/comecar.c) continue;; esac
  sources+=("$source")
done
BIN="${NUVIO_SHOT_BIN:-/tmp/nuvio-comecar-shot}"
cc -DAJUSTES_TESTE "${sources[@]}" tests/comecar_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
run_capture() {
  sandbox-exec -p '(version 1) (allow default) (deny network*)' "$BIN" "$1"
}
if [ "${NUVIO_J2_MATRIX:-0}" = 1 ]; then
  output=${1:-/tmp/nuvio-comecar}
  for glass in 0 1; do
    NUVIO_SHOT_VIDRO=$glass run_capture "$output/start-$glass-"
  done
else
  run_capture "${1:-/tmp/nuvio-comecar}"
fi
