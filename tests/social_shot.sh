#!/bin/bash
# Capturas do painel da tecla AZUL com a aba Social, em BMP, sem interacao.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/social_shot.sh /tmp/nuvio-social
#
# -DNV_REC_URL: a aba SO EXISTE com o servico compilado (recomenda_ativo). Sem
# esta bandeira a captura sairia mostrando o painel de antes — que e o
# comportamento certo do pacote sem servico, e nao o que esta foto quer provar.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-social-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados da captura em $NUVIO_DADOS"

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
# src/recomenda.c e INCLUIDO pelo teste, como em tests/recomenda.c: a lista de
# contatos e estatica do modulo e semea-la por dentro e o unico jeito de
# fotografar as telas de envio sem um servidor no ar.
cc "${sources[@]}" tests/social_shot.c -Isrc -o /tmp/nuvio-social-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-social-shot "$@"
