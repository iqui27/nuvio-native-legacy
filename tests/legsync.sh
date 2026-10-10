#!/bin/bash
# Cola de sessao do AutoSync (legsync.c) com referencia embutida real.
#   bash tests/legsync.sh    SANITIZE=1 (ASan/UBSan) ou SANITIZE=thread
set -euo pipefail
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}; FX="$T/nv-legref-fx"
bash tests/legref_fixtures.sh "$FX"
# Referência sem flag forced, mas só três eventos (como a faixa 7 da TCL).
python3 - "$FX" <<'PYFX'
from pathlib import Path
import sys
p = Path(sys.argv[1])
(p / 'sparse.srt').write_text('\n\n'.join((p / 'emb.srt').read_text().split('\n\n')[:3]) + '\n')
PYFX
mkvmerge -q -o "$FX/sparse.mkv" "$FX/video.mkv" --language 0:eng "$FX/sparse.srt"
D=$(mktemp -d "$T/nv-legsync.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
# LS_RITMO=0: sem o teto de 8 Ranges/s (o teste le do disco).
cc "${flags[@]}" -DLS_RITMO=0 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g -Wall -Wextra src/legsync.c src/legsyncui.c src/legenda2.c src/legref.c src/autosync.c src/audsync.c src/audvad.c src/legenda.c \
  src/assrender.c tests/legsync.c -pthread -lm -o "$D/t"
"$D/t" "$FX"

if [ -z "${TCL_CASO:-}" ]; then
  for caso in 1 2 3; do TCL_CASO=$caso "$D/t" "$FX"; done
fi
