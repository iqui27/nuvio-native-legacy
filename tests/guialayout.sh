#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-guialayout.XXXXXX")
trap 'rm -rf "$work"' EXIT
node tests/guialayout_html5.cjs
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
# guiav5_bugs.c exercises the app's production preview audio dispatch.
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
cache="${TMPDIR:-/tmp}/nuvio-guialayout-$(basename "$PWD")"
mkdir -p "$cache"
flags=(-Isrc -O0 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations)
objects=()
for source in "${sources[@]}"; do
  object="$cache/$(basename "$source").o"
  if [[ ! -f "$object" || "$source" -nt "$object" ]]; then
    cc "${flags[@]}" -c "$source" -o "$object"
  fi
  objects+=("$object")
done
cc "${flags[@]}" -I"$work" tests/guialayout.c "${objects[@]}" -o "$work/test" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/test"
video_objects=()
for object in "${objects[@]}"; do
  [[ "$object" == */player.c.o ]] || video_objects+=("$object")
done
cc "${flags[@]}" tests/guialayout_video.c src/guia.c src/guiav5ui.c "${video_objects[@]}" -o "$work/video" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
"$work/video"
bash tests/guiav5.sh
bash tests/guiav5_epg_lifetime.sh
