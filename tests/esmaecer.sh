#!/bin/bash
# Protecao de OLED: estagios, tecla que acorda e fator de brilho. Ver tests/esmaecer.c.
#   bash tests/esmaecer.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-esmaecer.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/esmaecer.c -Isrc -o "$work/test" -O1 -g -lm -Wno-macro-redefined
"$work/test"
