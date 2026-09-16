#!/bin/bash
# Listas da Biblioteca: leitura das respostas do Trakt e do Simkl a partir de
# fixture, o que fica gravado no perfil e o que chega na Home.
#
# NUVIO_DADOS APONTA PARA UMA PASTA TEMPORARIA, e o programa recusa rodar se
# dados_dir() nao for ela. Um teste deste repositorio ja sobrescreveu os dados
# reais do dono; a guarda esta nos dois lados de proposito.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-listas-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
# CHAVE DO TRAKT NO PACOTE. Um build de verdade recebe -DNV_TRAKT_CLIENT_ID de
# tools/env.sh, a partir de local.properties (que e do dono e nao esta aqui).
# Sem ela, `lst_aceita_home` recusa levar uma lista do Trakt para a Home — e
# recusa CERTO, porque /lists/<id>/items nao responde sem client id, que e a
# mesma guarda que vertudo.c ja tem (semFonte). O teste e do encanamento, nao do
# provisionamento da chave, entao ele configura o pacote como um pacote real.
cc "${sources[@]}" tests/listas.c -Isrc -o "$tmp/listas" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -Wall -Wextra -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/listas" "$@"
