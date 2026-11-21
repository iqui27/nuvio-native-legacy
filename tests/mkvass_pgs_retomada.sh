#!/bin/bash
# Retomada PGS no inicio exato do Cluster: a queda de folga durante a
# varredura nao pode adiar a legenda vigente (400 ms). Em 03d80de03 dava
# rc 1 (quadro chegava a 1,857 s, depois do fim real em 1,700 s).
#   bash tests/mkvass_pgs_retomada.sh        (SANITIZE=1 liga ASan+UBSan;
#                                             TSAN=1 liga Clang TSan)
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-retomada.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
if [ "${TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
# Variante "curta" do sparse: 2 SimpleBlocks de video de 700 KiB por Cluster
# (a leitura precisa de varios Ranges) e BlockDuration de 400 ms na PGS.
mkdir "$DIR/curta"
python3 - "$DIR/curta" <<'PY'
from pathlib import Path
fixture = Path("tests/mkvass_pgs_fixture.py").read_text()
block = "        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))"
assert fixture.count(block) == 1
assert fixture.count('u(0x9b,2000)') == 1
fixture = fixture.replace(block, block.replace('128*1024))', '700*1024))*2'))
fixture = fixture.replace('u(0x9b,2000)', 'u(0x9b,400)')
exec(compile(fixture, 'fixture-pgs-retomada', 'exec'))
PY
# Constantes de producao do ritmo: sem MKVASS_PGS_SCAN_MS reduzido.
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra \
  tests/mkvass_pgs_retomada.c src/{mkvass,pgs_mkv_restore,imgsub,pgs,legenda,assrender,dados}.c \
  tests/mkvass_i18n.c -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
echo "== retomada (pausa em 600,0 s; folga 30 -> 19.75; relogio 1x apos 1,3 s)"
NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/curta/sparse.mkv" 30
