#!/bin/bash
# S5: execute the production opening renderer with recorded text/draw calls.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=${TMPDIR:-/tmp}
mkdir -p "$TMPDIR"
work=$(mktemp -d "$TMPDIR/nuvio-player-loading.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 - "$work/render.c" <<'PY'
from pathlib import Path
import sys
s = Path('src/player.c').read_text()
a = s.index('static void desenharCinema(')
b = s.index('\nvoid player_desenhar(', a)
Path(sys.argv[1]).write_text(s[a:b])
a=s.index('int  player_carregando(void)')
b=s.index('\nint  player_controles_visiveis',a)
Path(sys.argv[1]).with_name('loading-state.c').write_text(s[a:b])
a=s.index('int  player_com_video(void)')
b=s.index('\n// Para o Discord',a)
Path(sys.argv[1]).with_name('video-state.c').write_text(s[a:b])
a=s.index('  GfxRect tela =',s.index('\nvoid player_desenhar('))
b=s.index('\n#ifdef NV_SHOT_HOOKS',a)
Path(sys.argv[1]).with_name('plane-state.c').write_text(
    'static int renderFura(void) {\n'+s[a:b]+'\nreturn furar;\n}\n')
PY
read -r -a sdl_flags <<< "$(pkg-config --cflags --libs sdl2)"
cc -DNV_NATIVE_SEEK_TEST tests/player_loading.c src/abrecine.c -I"$work" -Isrc \
  "${sdl_flags[@]}" -lm -o "$work/test"
"$work/test"
