#!/bin/bash
# Pedido de amizade na ilha do relogio: "dito" so depois de aparecer. Ver o .c.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ilha-pedidos.XXXXXXXX")
trap 'rm -rf "$dir"' EXIT
cc tests/ilha_pedidos.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wl,-undefined,dynamic_lookup -o "$dir/test" \
  -Wall -Wno-macro-redefined -Wno-deprecated-declarations -Wno-unused-function
"$dir/test"
