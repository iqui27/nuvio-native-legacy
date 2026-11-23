#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-/tmp}/nuvio-homeassistiu.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc -std=c99 -D_DEFAULT_SOURCE -Wall -Wextra -O1 -g -DNV_MAPA_PURO -DFIL_TESTE -Isrc \
  src/mapa.c src/js.c src/farolgosto.c src/fileiras.c tests/homeassistiu.c -pthread -lm -o "$bin"
"$bin"
