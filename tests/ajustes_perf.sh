#!/bin/bash
# Settings walk: every section, Down through every row, per-frame CPU ms, draw
# calls, text rasterizations and texture uploads (ajustes_perf.h).
#
#   bash tests/ajustes_perf.sh [outdir]          # numbers + frames.csv + PNGs
#   NUVIO_PERF_ASSERT=1 bash tests/ajustes_perf.sh  # FAIL if a settled scroll
#                                                  # frame rasterizes text
set -eu
cd "$(dirname "$0")/.."
out="${1:-/Volumes/ExternalSSD/tmp/rev/ajustesperf/check}"
obj=/tmp/nuvio-ajustes-perf-obj
mkdir -p "$out" "$obj"
rm -f "$obj"/*.o
flags="-DAJUSTES_TESTE -DAJUSTES_PERF -DglTexImage2D=perf_tex_upload -Isrc -O1 -g
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
  -Wno-deprecated-declarations -Wno-macro-redefined"
ls src/*.c src/dts/*.c tests/ajustes_shot.c tests/ajustes_perf_upload.c | grep -v '^src/main\.c$' |
  xargs -P "$(sysctl -n hw.ncpu)" -I{} sh -c 'cc -c '"$(echo $flags)"' "$1" -o '"$obj"'/$(echo "$1" | tr / _).o' _ {}
cc "$obj"/*.o -o /tmp/nuvio-ajustes-perf -L/opt/homebrew/lib \
  -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL
NUVIO_AJUSTES_PERF="$out" /tmp/nuvio-ajustes-perf
python3 tests/ajustes_perf.py "$out/frames.csv"
