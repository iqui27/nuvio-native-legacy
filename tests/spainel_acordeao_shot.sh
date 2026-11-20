#!/bin/bash
# Capturas do foco no topo da lista do painel da tecla azul (ver o .c).
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/social_shot.sh /var/folders/tb/3tdgxb_151x63831hfnszg1n9cg9nz/T//nuvio-social
#
# -DNV_REC_URL: a aba SO EXISTE com o servico compilado (recomenda_ativo). Sem
# esta bandeira a captura sairia mostrando o painel de antes — que e o
# comportamento certo do pacote sem servico, e nao o que esta foto quer provar.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /var/folders/tb/3tdgxb_151x63831hfnszg1n9cg9nz/T//nv-acordeao-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT
echo "dados da captura em $NUVIO_DADOS"

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/recomenda.c) continue;; esac
  sources+=("$source")
done
# src/recomenda.c e INCLUIDO pelo teste, como em tests/recomenda.c: a lista de
# contatos e estatica do modulo e semea-la por dentro e o unico jeito de
# fotografar as telas de envio sem um servidor no ar.
cc "${sources[@]}" tests/spainel_acordeao_shot.c -Isrc -o /var/folders/tb/3tdgxb_151x63831hfnszg1n9cg9nz/T//nv-acordeao-shot \
  -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/var/folders/tb/3tdgxb_151x63831hfnszg1n9cg9nz/T//nv-acordeao-shot "$@"
