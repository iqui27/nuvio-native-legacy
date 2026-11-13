#!/usr/bin/env bash
# DTS notice lifetime (review C-R1). Extracts the real DTS-notice block from
# src/player.c and drives it frame by frame: the per-frame plrilha request must
# last the whole notice window, a later failure on the same source must show,
# and seek/retry/track change must not repeat an already shown state.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dtsaviso.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/dts-aviso.c" <<'PY'
from pathlib import Path
import sys
s=Path('src/player.c').read_text()
assert 'void player_abrir(int indiceCatalogo, const char *url) {\n  dtsAvisoSessao++;' in s
assert s.count('dtsAvisoSessao++;') == 1, 'seek/retry must not reset notice history'
a=s.index('  { static int dtsVisto;');b=s.index('  // MINIATURAS FEITAS NO APARELHO',a)
Path(sys.argv[1]).write_text('static void dtsAvisoQuadro(Uint32 agora) {\n'+s[a:b]+'}\n')
PY
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -std=c11 -Wall -Wextra -Werror -Isrc -I"$work" tests/player_dts_aviso.c "${flags[@]}" -o "$work/test"
"$work/test"
