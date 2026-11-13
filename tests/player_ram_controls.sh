#!/usr/bin/env bash
# GREEN/F2 -> RAM overlay toggle (Matis94 C3). Extracts the real bufferRamTecla
# body out of src/player.c and exercises it against a fake settings pair.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ramctl.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/ram-controls.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/player.c').read_text()
a=s.index('static int bufferRamTecla(');b=s.index('\n#endif',a)
Path(sys.argv[1]).write_text(s[a:b])
assert s.count('ramoverlay_desenhar(agora, ajustes_ram_overlay());')==2
PY
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=c11 -Wall -Wextra -Werror -I"$work" tests/player_ram_controls.c "${flags[@]}" -o "$work/test"
"$work/test"
