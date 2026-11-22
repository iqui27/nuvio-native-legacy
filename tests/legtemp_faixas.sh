#!/bin/bash
# Legenda temporaria: a cola de faixas.c (tests/legtemp_faixas.c) sobre a regra pura (tests/legtemp.sh).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legtemp-faixas.XXXXXX")
trap 'rm -rf "$work"' EXIT
case "$(uname -s)" in Darwin) strip=-Wl,-dead_strip ;; *) strip=-Wl,--gc-sections ;; esac
cc tests/legtemp_faixas.c src/legtemp.c src/linguas.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  $(pkg-config --cflags sdl2 2>/dev/null || true) \
  -std=gnu11 -ffunction-sections -fdata-sections "$strip" \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -o "$work/t" -lm
"$work/t"
