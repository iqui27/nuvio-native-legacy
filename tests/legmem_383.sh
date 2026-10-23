#!/bin/bash
# #383: a legenda escolhida a mao vale no episodio seguinte mesmo com o audio
# no mesmo idioma. Ver tests/legmem_383.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legmem383.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/legmem_383.c -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -Wno-missing-field-initializers -o "$work/t"
"$work/t"
