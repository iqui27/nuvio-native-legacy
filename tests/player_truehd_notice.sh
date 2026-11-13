#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
python3 - "$work/truehd-render.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/player.c').read_text()
a=s.index('static void avisarTruehd(')
b=s.index('\n#endif',a)
Path(sys.argv[1]).write_text(s[a:b])
# Existing localized sentence must have exactly one replaceable codec token.
for p in Path('src').glob('idioma_*.h'):
    for line in p.read_text().splitlines():
        if '"Áudio DTS convertido nesta TV"' in line:
            assert line.count('DTS')==2,(p,line)
PY
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=c11 -Wall -Wextra -Werror -Isrc -I"$work" tests/player_truehd_notice.c "${sdl_flags[@]}" -o "$work/test"
"$work/test"
