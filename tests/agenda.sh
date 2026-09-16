#!/bin/bash
# Agenda: parse do corpo /tv/<id> do TMDB, a frase da tela de titulo, e os
# lembretes em disco (sobrevivencia e isolamento por perfil).
#
#   bash tests/agenda.sh
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o teste ABORTA se dados_dir()
# nao for exatamente ela. Um teste deste repositorio ja escreveu por cima dos
# dados reais do dono; a pasta some no fim, deu certo ou nao.
set -eu
cd "$(dirname "$0")/.."

NUVIO_DADOS=$(mktemp -d /tmp/nuvio-agenda-dados.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/agenda.c) continue;; esac
  sources+=("$source")
done
# src/agenda.c e INCLUIDO pelo teste: `lerCorpoTv` e estatico e e o parse de
# verdade, o mesmo que extras.c executa com o corpo vindo da rede.
cc "${sources[@]}" tests/agenda.c -Isrc -o /tmp/nuvio-agenda \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-agenda "$@"
