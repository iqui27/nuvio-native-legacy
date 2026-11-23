#!/bin/bash
# Ritmo do leitor PGS: queda de folga no meio do Cluster nao pode adiar a
# legenda vigente, e o ritmo recomeca depois do seek (sem divida de scanNext).
# Em 70fbc91f8 o modo "drop" dava rc 1 (quadro chegava aos 5 s, ja expirado).
#   bash tests/mkvass_pgs_ritmo.sh        (SANITIZE=1 liga ASan+UBSan)
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-ritmo.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
python3 tests/mkvass_pgs_fixture.py "$DIR"
# Variante "drop" do sparse: 2 SimpleBlocks de video de 700 KiB antes da
# primeira legenda — a leitura do Cluster precisa de varios Ranges.
mkdir "$DIR/drop"
python3 - "$DIR/drop" <<'PY'
from pathlib import Path
import sys
fixture = Path("tests/mkvass_pgs_fixture.py").read_text()
block = "        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))"
assert fixture.count(block) == 1
fixture = fixture.replace(block, block.replace("128*1024))", "700*1024))*2"))
exec(compile(fixture, "fixture-pgs-drop", "exec"))
PY
# Constantes de producao do ritmo: sem MKVASS_PGS_SCAN_MS reduzido.
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra \
  tests/mkvass_pgs_ritmo.c src/{mkvass,pgs_mkv_restore,imgsub,pgs,legenda,assrender,assperf,assfios,dados}.c \
  tests/mkvass_i18n.c -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
echo "== ritmo drop (folga 30 -> 19.75 durante o Cluster, relogio 1x)"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/drop/sparse.mkv" 30 drop
echo "== ritmo seek (600 -> 300 s, folga 3)"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/sparse.mkv" 3 seek
echo "== ritmo ahead (playhead 600 s, Cluster de 32 KiB, futuro 660 s)"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/ahead.mkv" 3 ahead
echo "== ritmo behind (playhead 599 s, Cluster futuro 600 s)"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/ahead.mkv" 3 behind
