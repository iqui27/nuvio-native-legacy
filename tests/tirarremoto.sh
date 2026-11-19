#!/bin/bash
# Marcar como assistido nao esconde a serie no Trakt; so "Tirar de Continuar".
#   bash tests/tirarremoto.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tirarremoto.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc src/trakt.c src/tirarremoto.c tests/stub_fichameta.c src/js.c src/jsw.c src/metaprov.c tests/tirarremoto.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -o "$dir/teste" -O1 -g -Wall -Wextra -Wno-deprecated-declarations -Wl,-dead_strip -lpthread
"$dir/teste" | tail -9
