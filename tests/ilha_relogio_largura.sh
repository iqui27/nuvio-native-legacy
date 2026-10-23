#!/bin/bash
# A reserva do menu do topo cobre a ilha real do relogio com o medidor ligado. Ver o .c.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ilha-relogio-largura.XXXXXXXX")
trap 'rm -rf "$dir"' EXIT
cc tests/ilha_relogio_largura.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -Wl,-undefined,dynamic_lookup -o "$dir/test" \
  -Wall -Wno-macro-redefined -Wno-deprecated-declarations -Wno-unused-function
"$dir/test"
