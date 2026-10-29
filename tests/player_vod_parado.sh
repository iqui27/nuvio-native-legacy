#!/bin/bash
# Row 11 (mapa-erros-logs): "[player] VOD parado" saia UM parágrafo por PARADA,
# nao por quadro. O harness /tmp/repro-vodparado contava 1300 linhas em 30 s de
# VOD parado a 60 fps; a evidencia do #681 era cortada do log.
#   bash tests/player_vod_parado.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-vod-parado.XXXXXX")
trap 'rm -rf "$work"' EXIT
# O bloco de produção, extraido de src/player.c: nada de cópia que envelhece.
python3 - "$work/bloco.inc" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
a = s.index('{ static Uint32 vodPosEm;')
b = s.index('\n      } }\n', a) + len('\n      } }\n')
Path(sys.argv[1]).write_text(s[a:b])
PY
cc -O1 -g -Isrc -I"$work" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  tests/player_vod_parado.c -o "$work/test" -Wno-deprecated-declarations
"$work/test" "$work/saida.txt"
echo "player_vod_parado: PASS"
