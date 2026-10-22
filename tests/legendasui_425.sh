#!/bin/bash
# Subtitle selector model (src/legendasui.c). See tests/legendasui_425.c.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legendasui.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc ${flags[@]+"${flags[@]}"} tests/legendasui_425.c src/linguas.c src/bidi.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -g -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t"
rc=0
for caso in lista direita segurar voltar atraso ponteiro; do "$work/t" "$caso" || rc=1; done
exit "$rc"
