#!/bin/bash
# Legacy outline colour must never override an explicit Black border.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-borda-preta.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work" <<'PY'
from pathlib import Path
import sys
work = Path(sys.argv[1])
s = Path('src/player.c').read_text()
start = s.index('void player_dir(')
end = s.index('// Proporcao do QUADRO', start)
(work / 'prefs.inc').write_text(s[start:end])
s = Path('src/faixas.c').read_text()
start = s.index('static void ciclarEstilo(')
end = s.index('// Rotulo da linha', start)
key_start = s.index('static int estiloTecla(')
key_end = s.index('\nvoid faixas_evento(', key_start)
(work / 'picker.inc').write_text(s[start:end] + s[key_start:key_end])
PY
cc tests/legenda_borda_preta.c -I"$work" -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -std=gnu11 -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -Wno-macro-redefined -Wno-deprecated-declarations -o "$work/t"
"$work/t" "$work"
