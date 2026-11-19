#!/bin/bash
# #202: latencia por add-on, prazo da abertura e explicacao da ilha (puro).
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-inicio.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -O1 -g -Wall -Wextra -DNV_ADDONSTATS_PURO -Isrc \
  src/addonstats.c src/inicio.c tests/inicio_rapido.c -o "$dir/t" -lpthread
"$dir/t"
