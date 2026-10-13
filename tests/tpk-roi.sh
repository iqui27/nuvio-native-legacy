#!/bin/bash
# Sem TV: ROI do .tpk sempre dentro da tela no padrao; o canario
# (NV_TPK_ZOOM_ROI=1) ainda deixa o zoom de antes passar.
set -eu
cd "$(dirname "$0")/.."
SDL="-I/opt/homebrew/include $(sdl2-config --cflags) $(sdl2-config --libs)"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-card-hole.XXXXXXXX")
trap 'rm -rf "$work"' EXIT
# Compile the production bodies against the instrumented GL boundary.
python3 - "$work" <<'PY'
from pathlib import Path
import sys
out = Path(sys.argv[1]) / 'tpk-card-hole-real.h'
parts = []
for path, start, end in [('src/gfx.c', 'void gfx_furo(', '\nvoid gfx_furo_raio('),
                         ('src/trailer.c', 'void trailer_recorte(', '\n#ifdef __EMSCRIPTEN__')]:
    source = Path(path).read_text()
    first = source.index(start)
    parts.append(source[first:source.index(end, first)])
out.write_text('\n'.join(parts))
PY
cc -O1 -g -Wall -DNV_TPK -Isrc -I"$work" tests/tpk-card-hole.c src/video_tpk.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread $SDL -o "$work/hole"
"$work/hole"
cc -O1 -g -Wall -DNV_TPK -Isrc tests/tpk-roi.c src/video_tpk.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread $SDL -o /tmp/nuvio-tpk-roi
/tmp/nuvio-tpk-roi
cc -O1 -g -Wall -DNV_TPK -DNV_TPK_ZOOM_ROI=1 -Isrc tests/tpk-roi.c src/video_tpk.c src/vazao.c src/velocidade.c src/faixasmkv.c src/audioinfo.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread $SDL -o /tmp/nuvio-tpk-roi-canario
/tmp/nuvio-tpk-roi-canario
