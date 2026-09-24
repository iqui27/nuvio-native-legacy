#!/bin/bash
# GIF no navegador: o gif_textura NOVO (decode em C, pthread) contra o da 1.4.6
# (quadro remontado e decodificado pelo navegador no Worker), no Chrome
# headless, com WebGL e pthreads de verdade. Ver tests/gifbanco_tizen.c.
#
#   bash tests/gifbanco-tizen.sh
#   NV_GIFBANCO_REF=v1.4.6 bash tests/gifbanco-tizen.sh    # outra base do "antigo"
#
# Imprime uma linha BANCO por GIF e variante, e a linha "[gif] deu a volta" de
# cada uma. O Chrome do Mac e varias vezes mais rapido que a CPU da AU7000: o
# que vale daqui e a PROPORCAO entre as duas variantes e o custo no fio
# principal, nao os milissegundos absolutos.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
EMCC="$EMSDK_DIR/upstream/emscripten/emcc"
CHROME="${NV_CHROME:-/Applications/Google Chrome.app/Contents/MacOS/Google Chrome}"
[ -x "$EMCC" ] || { echo "gifbanco-tizen.sh: emcc nao encontrado em $EMSDK_DIR" >&2; exit 127; }
[ -x "$CHROME" ] || { echo "gifbanco-tizen.sh: Chrome nao encontrado ($CHROME)" >&2; exit 127; }
GIFS=$(bash tests/gifbanco.sh --so-gerar)
OUT="${TMPDIR:-/tmp}/nuvio-gifbanco-tizen"
REF="${NV_GIFBANCO_REF:-master}"
rm -rf "$OUT"; mkdir -p "$OUT/antigo/src" "$OUT/novo/src" "$OUT/antigo/tests" "$OUT/novo/tests" "$OUT/antigo0/src" "$OUT/antigo0/tests"
git show "$REF:src/gif.c" > "$OUT/antigo/src/gif.c"
git show "$REF:src/gif.h" > "$OUT/antigo/src/gif.h"
git show "$REF:tools/decodificador.js" > "$OUT/antigo/decodificador.js"
# O Worker antigo ganha um cronometro: do pedido do quadro ao ImageBitmap
# composto pronto (decode do navegador + composicao + reducao), por GIF. E o
# numero que se compara ao "decode X ms por quadro" do fio novo.
python3 - "$OUT/antigo/decodificador.js" <<'PY'
import sys
p = sys.argv[1]; s = open(p).read()
a = "  createImageBitmap(g.frames[i]).then(function (bmp) {"
b = "    self.postMessage({ gifPronto: { id: m.id, i: i }, bitmap: ib }, [ib]);"
c = "function gifSoltar(m) { delete gifs[m.id]; }"
assert a in s and b in s and c in s, "decodificador.js de referencia mudou"
s = s.replace(a, "  var t0 = performance.now();\n" + a)
s = s.replace(b, "    self.nvMs = (self.nvMs || 0) + performance.now() - t0; self.nvN = (self.nvN || 0) + 1;\n" + b)
s = s.replace(c, "function gifSoltar(m) { if (self.nvN) fetch('/log', { method: 'POST', body: 'WORKER ' + self.nvN + ' quadros, ' + (self.nvMs / self.nvN).toFixed(1) + ' ms por quadro (pedido ate bitmap pronto)' }); self.nvMs = 0; self.nvN = 0; delete gifs[m.id]; }")
open(p, 'w').write(s)
PY
cp "$OUT/antigo/src/gif.c" "$OUT/antigo/src/gif.h" "$OUT/antigo0/src/"
cp "$OUT/antigo/decodificador.js" "$OUT/antigo0/"
cp src/gif.c src/gif.h "$OUT/novo/src/"
cp tools/decodificador.js "$OUT/novo/decodificador.js"

cat > "$OUT/pre.js" <<'JS'
Module.nvVoltas = 0;
Module.print = function (t) {
  if (t.indexOf('deu a volta') >= 0) Module.nvVoltas++;
  if (t.indexOf('[gif]') >= 0 || t.indexOf('BANCO') >= 0 || t === 'FIM')
    fetch('/log', { method: 'POST', body: t });
  console.log(t);
};
Module.printErr = function (t) { console.warn(t); };
Module.nvFim = function () { setTimeout(function () { fetch('/log', { method: 'POST', body: 'PAGINA_FIM' }); }, 200); };
JS

for v in antigo antigo0 novo; do
  d="$OUT/$v"
  cp tests/gifbanco_tizen.c "$d/tests/"
  def=""; [ "$v" = antigo ] && def="-DNV_BANCO_ANTIGO"
  [ "$v" = antigo0 ] && def="-DNV_BANCO_ANTIGO -DPASSO_MS=0.0"
  "$EMCC" "$d/tests/gifbanco_tizen.c" "$d/src/gif.c" -I"$d/src" -Isrc -O2 $def -o "$d/index.html" \
    -pthread -sPTHREAD_POOL_SIZE=4 -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 \
    -sMAX_WEBGL_VERSION=1 -sEXIT_RUNTIME=0 -sWASM_BIGINT=0 \
    --pre-js "$OUT/pre.js" \
    --preload-file "$GIFS/a35.gif@/a35.gif" --preload-file "$GIFS/b75.gif@/b75.gif" \
    --preload-file "$GIFS/c198.gif@/c198.gif" \
    -Wno-deprecated-declarations -Wno-macro-redefined > "$d/emcc.log" 2>&1 ||
    { grep -E "error" "$d/emcc.log" | head -20; exit 1; }
done

PORTA=${NUVIO_PORTA:-8797}
python3 - "$OUT" "$PORTA" > "$OUT/servidor.log" 2>&1 <<'PY' &
import functools, http.server, socketserver, sys, os
raiz, porta = sys.argv[1], int(sys.argv[2])
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()
    def do_POST(self):
        n = int(self.headers.get('Content-Length', 0))
        t = self.rfile.read(n).decode('utf-8', 'replace')
        with open(os.path.join(raiz, 'log.txt'), 'a') as f: f.write(t + '\n')
        self.send_response(204); self.end_headers()
    def log_message(self, *a): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', porta), functools.partial(H, directory=raiz)) as s:
    s.serve_forever()
PY
SERV=$!
trap 'kill $SERV 2>/dev/null || true' EXIT
sleep 1

for v in antigo antigo0 novo; do
  : > "$OUT/log.txt"
  "$CHROME" --headless=new --no-first-run --no-default-browser-check \
    --user-data-dir="$OUT/perfil-$v" --use-angle=swiftshader --enable-unsafe-swiftshader \
    --window-size=1280,720 "http://127.0.0.1:$PORTA/$v/index.html" > "$OUT/chrome-$v.log" 2>&1 &
  CR=$!
  for _ in $(seq 1 240); do
    grep -q PAGINA_FIM "$OUT/log.txt" 2>/dev/null && break
    sleep 1
  done
  kill $CR 2>/dev/null || true
  wait $CR 2>/dev/null || true
  echo "== $v"
  grep -E "BANCO|deu a volta|WORKER" "$OUT/log.txt" || { echo "  (sem resultado; ver $OUT/chrome-$v.log)"; }
done
