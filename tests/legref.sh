#!/bin/bash
# Coletor independente da referencia do AutoSync contra MKV reais.
#   bash tests/legref.sh    SANITIZE=1 (ASan/UBSan) ou SANITIZE=thread
set -euo pipefail
cd "$(dirname "$0")/.."
T=${TMPDIR:-/tmp}; FX="$T/nv-legref-fx"
bash tests/legref_fixtures.sh "$FX"
D=$(mktemp -d "$T/nv-legref.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
elif [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
# O coletor NAO pode alcancar o estado principal: nem mkvass, nem a carga do
# overlay, nem libass, nem o backend de video.
cc -Isrc -c -Wall -Wextra -Werror src/legref.c -o "$D/legref.o"
if nm -u "$D/legref.o" | grep -E '_(mkvass_|legenda_carregar|legenda_definir|legenda_atualizar|legenda_desligar|assrender_|video_)'; then
  echo "legref.c chama estado do overlay principal"; exit 1; fi
cc "${flags[@]}" -Isrc -O1 -g -Wall -Wextra src/legref.c src/legenda.c src/assrender.c tests/legref.c \
  -pthread -lm -o "$D/t"
"$D/t" "$FX"
# HTTP REAL (rede.c + libcurl) contra servidor Range local: completo, redirect
# para outra origem, e servidor que ignora Range (recusa sem baixar o arquivo).
cc -Isrc -I/opt/homebrew/include -O1 -g src/legref.c src/legenda.c src/assrender.c src/rede.c src/redeurl.c \
  src/dados.c tests/legref_http.c -pthread -lm -lz -Wno-deprecated-declarations -o "$D/http"
P=$((20000 + RANDOM % 20000))
python3 tests/legref_rangesrv.py "$FX" $P & S1=$!
python3 tests/legref_rangesrv.py "$FX" $((P + 1)) semrange & S2=$!
trap 'kill $S1 $S2 2>/dev/null || true; rm -rf "$D"' EXIT
for i in $(seq 50); do nc -z 127.0.0.1 $((P + 1)) 2>/dev/null && nc -z 127.0.0.1 $P 2>/dev/null && break; sleep 0.1; done
"$D/http" "http://127.0.0.1:$P/f/ff.mkv" 2>/dev/null | tee "$D/h1" | grep -q 'fase=2 motivo=ok doc=1 n=113' || { cat "$D/h1"; echo "http completo falhou"; exit 1; }
"$D/http" "http://127.0.0.1:$P/redir/ff.mkv" 2>/dev/null | tee "$D/h2" | grep -q 'fase=2 motivo=ok doc=1 n=113' || { cat "$D/h2"; echo "http redirect falhou"; exit 1; }
"$D/http" "http://127.0.0.1:$((P + 1))/f/ff.mkv" 2>/dev/null | tee "$D/h3" | grep -q 'motivo=no_range doc=0 n=0 pedidos=1 bytes=0' || { cat "$D/h3"; echo "http sem Range falhou"; exit 1; }
# Fonte de addon com proxyHeaders: sem os cabecalhos do player o CDN recusa.
"$D/http" "http://127.0.0.1:$P/ref/ff.mkv" 2>/dev/null | tee "$D/h4" | grep -q 'motivo=network' || { cat "$D/h4"; echo "http sem Referer devia falhar"; exit 1; }
CABS=$'User-Agent: Nuvio\nReferer: https://addon.test/' "$D/http" "http://127.0.0.1:$P/ref/ff.mkv" 2>/dev/null | tee "$D/h5" | grep -q 'fase=2 motivo=ok doc=1 n=113' || { cat "$D/h5"; echo "http com Referer do addon falhou"; exit 1; }
echo "legref http: completo, redirect, sem-Range e cabecalhos do addon ok"
