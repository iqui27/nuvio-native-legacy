#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legpre.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
fi
python3 - "$work/fonte.h" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
a = s.index('void player_definir_fonte(')
b = s.index('\nvoid player_voltar_a_esperar(', a)
Path(sys.argv[1]).write_text(s[a:b])
PY
cc ${flags[@]+"${flags[@]}"} tests/legpre.c src/legenda.c src/assrender.c -I"$work" \
  -Isrc -Itests -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -g -O1 -pthread -lm -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wall -Wextra -Wno-unused-function -Wno-macro-redefined -Wno-missing-field-initializers \
  -o "$work/t"
"$work/t"
