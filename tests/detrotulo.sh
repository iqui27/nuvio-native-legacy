#!/bin/bash
# Rotulo do botao primario do detalhe: filme assistido e "Reproduzir".
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-detrotulo.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -O1 -g -Wall -Wextra -Isrc tests/detrotulo.c -o "$work/test"
"$work/test"
