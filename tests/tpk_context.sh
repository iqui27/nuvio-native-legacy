#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tpk-context.XXXXXX")
trap 'rm -rf "$work"' EXIT
# Exercise the production bridge, without pulling in the Samsung host runtime.
python3 - "$work" <<'PY'
from pathlib import Path
import sys
work = Path(sys.argv[1])
source = Path('src/tpk.c').read_text()
start = source.index('void *tpk_gl_corrente(void) {')
work.joinpath('tpk_context_bridge.inc').write_text(source[start:source.index('\n}\n', start) + 3])
# Reuse the existing platform guard fixture for gpunivel.c:214 and its EGL path.
fixture = Path('tests/livepr2_platform.sh').read_text()
start = fixture.index("source = pathlib.Path('src/gpunivel.c').read_text()")
end = fixture.index('sys.exit(int(failed))', start)
work.joinpath('upscale.py').write_text(
    'import pathlib, subprocess, sys\nwork = pathlib.Path(sys.argv[1])\nfailed = False\n'
    + fixture[start:end] + 'sys.exit(int(failed))\n')
PY
python3 "$work/upscale.py" "$work"
# Only the cache fixture uses NV_TPK; other app objects supply normal host deps.
cc -c tests/tpk_context.c -o "$work/proof.o" -O1 -DNV_TPK \
  -include src/tpk.h -Isrc -I"$work" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/tex_cache.c) continue;; esac
  sources+=("$source")
done
cc "${sources[@]}" "$work/proof.o" -o "$work/proof" -O1 \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations
"$work/proof"
