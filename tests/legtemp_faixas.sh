#!/bin/bash
# Legenda temporaria: a cola de faixas.c (tests/legtemp_faixas.c) sobre a regra pura (tests/legtemp.sh).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legtemp-faixas.XXXXXX")
trap 'rm -rf "$work"' EXIT
case "$(uname -s)" in Darwin) strip=-Wl,-dead_strip ;; *) strip=-Wl,--gc-sections ;; esac
flags=(-std=gnu11 -ffunction-sections -fdata-sections -Wall -Wextra -Wno-unused-function -Wno-macro-redefined)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
"${CC:-cc}" "${flags[@]}" tests/legtemp_faixas.c src/legtemp.c src/linguas.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  $(pkg-config --cflags sdl2 2>/dev/null || true) \
  "$strip" -o "$work/t" -lm
"$work/t"
