#!/bin/bash
# Capturas da tela "Resolver um problema" (2.1) em PNG 1920x1080: o sintoma, os
# testes (fonte, add-ons, TV, nada achado, passo a passo), o registro enviado, a
# lista dos enviados e o envio que falhou. Antes, as regras por evento de tecla
# (foco inicial, parada na primeira falha, pedido de Fontes, o codigo que chega,
# remover da lista, guarda de 30 dias) e a varredura de navegacao D-pad. Janela
# GL ESCONDIDA e desenho num FBO: nada aparece na tela de quem roda. Fica fora da
# suite (*_shot.sh): precisa de GL. RESOLVER_TESTE nao envia nada de verdade.
#
#   bash tests/resolver_shot.sh tests/out/resolver
set -eu
cd "$(dirname "$0")/.."
NUVIO_DADOS=$(mktemp -d /tmp/nuvio-resolver-shot.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in */main.c) continue;; esac
  sources+=("$source")
done
BIN="${NUVIO_SHOT_BIN:-/tmp/nuvio-resolver-shot}"
mkdir -p "$(dirname "${1:-/tmp/nuvio-resolver}")"
cc -DAJUSTES_TESTE -DAVISOS_TESTE_ENVIO -DRESOLVER_TESTE "${sources[@]}" tests/resolver_shot.c -Isrc -o "$BIN" \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
"$BIN" "${1:-/tmp/nuvio-resolver}"
bash "$(dirname "$0")/resolver_retrato.sh"
