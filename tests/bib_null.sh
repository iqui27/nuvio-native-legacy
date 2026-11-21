#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# F61: montarBiblioteca com cat_item devolvendo NULL no meio do laco de
# ordenacao A-Z/Ano (troca do catalogo publica n=0). A-Z na base caia com
# rc 139; na ponta sobrevive. Ver tests/bib_null.c.
#
#   bash tests/bib_null.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bibnull-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/home.c) continue;; esac
  sources+=("$source")
done
mkdir -p "$tmp/dados"
for ordem in AZ ANO LISTA; do
  cc -DORDEM_PROBE=FIL_ORD_$ordem "${sources[@]}" tests/bib_null.c -Isrc -o "$tmp/t-$ordem" \
    -DNV_TRAKT_CLIENT_ID='"chave-de-teste"' -O1 -g \
    -Wl,--wrap=cat_item -Wl,--wrap=fil_ordem -Wl,--wrap=ov_ativa \
    -Wl,--wrap=ajustes_fonte_biblioteca -Wl,--wrap=ajustes_itens_fileira \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
  NUVIO_DADOS="$tmp/dados" "$tmp/t-$ordem"
done
