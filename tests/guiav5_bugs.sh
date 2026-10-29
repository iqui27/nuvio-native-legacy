#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-g5-bugs.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work"
# Exercise the app's actual preview audio dispatch with a recording player boundary.
python3 - "$work/guide_audio.inc" <<'PY'
from pathlib import Path
import sys
source = Path('src/app.c').read_text()
start = source.index('  {static int last_volume=-1,last_active,was_muted;')
end = source.index('  if (guia_pediu_parar_preview()', start)
Path(sys.argv[1]).write_text(source[start:end])
PY
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/guia.c|src/guiav5ui.c) continue;; esac
  sources+=("$source")
done
if [[ ${GUIAV5_BASELINE:-0} == 1 ]]; then
  git show HEAD:src/guiav5ui.c > "$work/guiav5ui.c"
  extra=(-DGUIAV5_UNDER_TEST=\"$work/guiav5ui.c\")
else
  extra=()
fi
cc "${sources[@]}" tests/guiav5_bugs.c ${extra[@]+"${extra[@]}"} -Isrc -I"$work" -o "$work/test" -O0 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL -Wno-deprecated-declarations
"$work/test"
