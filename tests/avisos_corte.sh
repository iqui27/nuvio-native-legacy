#!/bin/bash
# Corte cabeca+cauda do envio automatico (#203). Sem SDL nem rede.
#   bash tests/avisos_corte.sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-corte-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Wall -Wextra tests/avisos_corte.c -o "$tmp/t"
"$tmp/t"
