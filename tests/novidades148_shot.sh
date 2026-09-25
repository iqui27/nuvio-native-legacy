#!/bin/bash
# Capturas do cartao da 1.4.8 em BMP: a previa da cor viva em tres momentos do
# ciclo (primeiro titulo, meio da passagem, terceiro titulo) em pt, o segundo
# titulo e o foco em "Testar a velocidade" em en, e animacoes reduzidas. Antes,
# as regras: OK abre a cor, Esquerda + OK o teste de velocidade, Voltar = Agora
# nao, e todos gravam a marca da 1.4.8. Janela GL ESCONDIDA e desenho num FBO:
# nada aparece na tela de quem roda. Fica fora da suite (*_shot.sh): precisa de
# GL e de olho humano. Sem rede: as artes sao as do pacote (deploy/app/art).
#
#   bash tests/novidades148_shot.sh /tmp/nuvio-novidades148
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-n148-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/novidades148_shot.c -Isrc \
  -o /tmp/nuvio-n148-shot -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-n148-shot "${1:-/tmp/nuvio-novidades148}"
