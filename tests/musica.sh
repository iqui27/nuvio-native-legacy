#!/bin/bash
# Musicas: parse da resposta do Worker, URL do QR e lista Guardadas por perfil.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-musica-test"
cc -Wall -Wextra -O2 -DNV_REC_URL='""' tests/musica.c src/musica.c src/js.c -Isrc -o "$out" -lpthread
"$out"
rm -f "$out"
