#!/bin/bash
# #762: listas do MDBList na Biblioteca. Padrao de tests/listas.sh, SEM REDE:
# a leitura entra por fixture; fixar/Home e tudo local.
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mdblist-listas-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" tests/mdblist_listas.c -Isrc -o "$tmp/mdblist_listas" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -Wall -Wextra -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
mkdir -p "$tmp/dados"
NUVIO_DADOS="$tmp/dados" "$tmp/mdblist_listas" "$tmp/dados"
