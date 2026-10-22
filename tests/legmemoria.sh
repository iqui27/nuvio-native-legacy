#!/bin/bash
# A legenda escolhida a mao volta (2.0.3). Ver tests/legmemoria.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legmemoria.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/legmemoria.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -Wno-missing-field-initializers -o "$work/t"
saida=$("$work/t")
printf '%s\n' "$saida"
# A linha que faltou no log da TCL.
printf '%s\n' "$saida" | grep -q "\[legenda\] automatica: ultima escolha 'pob' -> embutida 2"
printf '%s\n' "$saida" | grep -q "\[legenda\] automatica: ultima escolha 'none' -> nenhuma"
