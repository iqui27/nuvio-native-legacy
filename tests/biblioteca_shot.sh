#!/bin/bash
# Capturas da Biblioteca renovada, em BMP, sem interacao e sem rede.
# Nao entra na suite: precisa de janela GL e de olho humano para julgar.
#
#   bash tests/biblioteca_shot.sh /tmp/nuvio-biblioteca
#
# NUVIO_DADOS e uma pasta temporaria e o programa recusa rodar se dados_dir()
# nao for ela: fixar uma lista GRAVA.
set -eu
cd "$(dirname "$0")/.."
saida="${1:-/tmp/nuvio-biblioteca}"
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibshot-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c; do
  if [ "$source" != src/main.c ]; then sources+=("$source"); fi
done
cc "${sources[@]}" tests/biblioteca_shot.c -Isrc -o "$tmp/shot" \
  -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS="$tmp/dados" "$tmp/shot" "$saida"
# SEGUNDA VOLTA so para a MEDIDA: comeca em lista, com o cache de textura frio.
# Ver a nota em tests/biblioteca_shot.c sobre por que uma volta so nao mede.
echo "--- segunda volta: lista com o cache frio ---"
NUVIO_DADOS="$tmp/dados2" "$tmp/shot" "$tmp/descarte" lista | grep "\[arte\]"
