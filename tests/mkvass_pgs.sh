#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d /tmp/nuvio-mkvass-pgs.XXXXXX)
SRV=
trap '[ -z "$SRV" ] || kill "$SRV" 2>/dev/null; rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
# MKV minimo com uma faixa S_HDMV/PGS: um display set (objeto 6x2 em 1920x1080)
# e outro que so limpa a tela. Sem video real. Um arquivo por modo de
# ContentEncoding: sem compressao, zlib (padrao em muitos remuxes) e cabecalho
# removido.
python3 tests/mkvass_pgs_fixture.py "$DIR"
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -lz -o "$DIR/test"
mkdir "$DIR/dados"
for modo in plain zlib strip plain-scan zlib-scan strip-scan; do
  echo "== PGS $modo"
  NUVIO_DADOS="$DIR/dados" "$DIR/test" "$DIR/$modo.mkv"
done
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -DMKVASS_RANGES_POR_SEG=1000 -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs_reliability.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -lz -o "$DIR/reliability"
for scenario in sparse empty malformed badzlib truncated nocues http; do
  file="$scenario"; [ "$scenario" != http ] || file=sparse
  echo "== reliability $scenario"
  NUVIO_DADOS="$DIR/dados" "$DIR/reliability" "$DIR/$file.mkv" "$scenario"
done

cc ${flags[@]+"${flags[@]}"} -Isrc -I/opt/homebrew/include -O1 -g -Wno-deprecated-declarations -DMKVASS_PGS_SCAN_MS=50 tests/mkvass_pgs_http.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c src/rede.c src/redeurl.c -lz -lpthread -o "$DIR/http"
python3 tests/servidor_range.py "$DIR" > "$DIR/server.log" 2>&1 &
SRV=$!
for ((i=0; i<100; i++)); do
  if rg -q '^porta ' "$DIR/server.log"; then break; fi
  sleep 0.02
done
PORT=$(awk '/^porta /{print $2; exit}' "$DIR/server.log")
test -n "$PORT"
for route in redir/sparse norange/sparse missing; do
  expected=0; [ "$route" != redir/sparse ] || expected=1
  NUVIO_DADOS="$DIR/dados" "$DIR/http" "http://127.0.0.1:$PORT/$route.mkv" "$expected"
done
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra tests/mkvass_pgs_budget.c src/mkvass.c src/imgsub.c src/pgs.c src/legenda.c tests/mkvass_i18n.c src/assrender.c src/dados.c -lz -o "$DIR/budget"
NUVIO_DADOS="$DIR/dados" "$DIR/budget" "$DIR/sparse.mkv"
