#!/bin/bash
# #370: o libass do wasm (.wgt) precisa da pasta /app/fonts para achar o Noto Naskh.
# Ver tests/ass_pasta_wasm.c. A funcao testada nao depende do libass.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ass-pasta-wasm.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -Isrc tests/ass_pasta_wasm.c src/assrender.c -o "$work/t" -Wall
"$work/t"
