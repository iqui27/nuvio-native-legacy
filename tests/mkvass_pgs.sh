#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvass-pgs.XXXXXX")
# A coleta do diagnostico e opt-in: estes testes leem mkvass_diagnostico.
export NUVIO_PGS_DIAGNOSTICS=1
SRV=
trap '[ -z "$SRV" ] || kill "$SRV" 2>/dev/null; rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
# MKV minimo com uma faixa S_HDMV/PGS: um display set (objeto 6x2 em 1920x1080)
# e outro que so limpa a tela. Sem video real. Um arquivo por modo de
# ContentEncoding: sem compressao, zlib (padrao em muitos remuxes) e cabecalho
# removido.
python3 tests/mkvass_pgs_fixture.py "$DIR"
SRC="src/mkvass.c src/pgs_mkv_restore.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c"
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs.c $SRC -lz -lm -o "$DIR/test"
mkdir "$DIR/dados"
for modo in plain zlib strip plain-scan zlib-scan strip-scan; do
  echo "== PGS $modo"
  NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/$modo.mkv"
done
# Variante "paced" do sparse: 5 SimpleBlocks de video de 128 KiB por Cluster —
# leitura contigua longa o bastante para o ritmo serial mostrar Ranges de
# 64 KiB consecutivos quando a varredura esta a frente do playhead.
mkdir "$DIR/paced"
python3 - "$DIR/paced" <<'PY'
from pathlib import Path
fixture = Path("tests/mkvass_pgs_fixture.py").read_text()
block = "        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))"
assert fixture.count(block) == 1
fixture = fixture.replace(block, block + "*5")
exec(compile(fixture, "fixture-pgs-paced", "exec"))
PY
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs_reliability.c $SRC -lz -lm -o "$DIR/reliability"
for scenario in sparse empty malformed badzlib truncated nocues http paced; do
  file="$scenario"; [ "$scenario" != http ] || file=sparse
  [ "$scenario" != paced ] || file=paced/sparse
  echo "== reliability $scenario"
  NUVIO_DADOS="$DIR/dados" "$DIR/reliability" "$DIR/$file.mkv" "$scenario"
done

cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wno-deprecated-declarations -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs_http.c $SRC src/rede.c src/redeurl.c -lz -lpthread -ldl -lm -o "$DIR/http"
python3 tests/servidor_range.py "$DIR" > "$DIR/server.log" 2>&1 &
SRV=$!
for ((i=0; i<100; i++)); do
  if rg -q '^porta ' "$DIR/server.log" 2>/dev/null; then break; fi
  sleep 0.02
done
PORT=$(awk '/^porta /{print $2; exit}' "$DIR/server.log")
test -n "$PORT"
for route in redir/sparse norange/sparse missing; do
  expected=0; [ "$route" != redir/sparse ] || expected=1
  NUVIO_DADOS="$DIR/dados" "$DIR/http" "http://127.0.0.1:$PORT/$route.mkv" "$expected"
done
kill "$SRV" 2>/dev/null; SRV=
# Fixture do budget: a varredura no limite exato do Cluster (pos==varreTs)
# nao estaciona mais — a PGS vigente pode estar nele. O budget e medido num
# Cluster ADIANTE dentro da janela (660 s com o playhead em 600), com a PGS
# logo no inicio dele — o primeiro Range futuro ja e ritmado, entao ela
# chega no pedaco de 64 KiB que abre o Cluster.
mkdir "$DIR/budget-fixture"
python3 - "$DIR/budget-fixture" <<'PY'
from pathlib import Path
fixture = Path("tests/mkvass_pgs_fixture.py").read_text()
block = "        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))"
assert fixture.count(block) == 1
fixture = fixture.replace(block, block + "*4")
secs = "for sec in (0,60,120,300,600):"
assert fixture.count(secs) == 1
fixture = fixture.replace(secs, "for sec in (0,60,120,300,600,660):")
fixture = fixture.replace("zip((0,60,120,300,600),offsets)",
                          "zip((0,60,120,300,600,660),offsets)")
alvo = ("        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))*4\n"
        "        if sec in (300,600) and kind != 'empty':\n"
        "            packet = mostra if kind not in ('malformed','badzlib') else b'\\x16\\xff\\xff'\n"
        "            body+=el(0xa0,el(0xa1,b'\\x86\\0\\0\\0'+packet)+u(0x9b,2000))\n"
        "            body+=el(0xa3,b'\\x86'+(3000).to_bytes(2,'big')+b'\\0'+limpa)")
novo = ("        if sec == 660:\n"
        "            body+=el(0xa0,el(0xa1,b'\\x86\\0\\0\\0'+mostra)+u(0x9b,2000))\n"
        "            body+=el(0xa3,b'\\x86'+(3000).to_bytes(2,'big')+b'\\0'+limpa)\n"
        "        body+=el(0xa3,b'\\x81\\0\\0\\0'+b'\\0'*(128*1024))*4")
assert fixture.count(alvo) == 1
fixture = fixture.replace(alvo, novo)
fixture = fixture.replace(
    "for kind in ('sparse','empty','malformed','badzlib','truncated'):",
    "for kind in ('sparse',):")
exec(compile(fixture, "fixture-pgs-budget", "exec"))
PY
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra tests/mkvass_pgs_budget.c $SRC -lz -lm -o "$DIR/budget"
NUVIO_DADOS="$DIR/dados" "$DIR/budget" "$DIR/budget-fixture/sparse.mkv"
