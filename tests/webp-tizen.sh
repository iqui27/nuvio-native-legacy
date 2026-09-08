#!/bin/bash
# Compila tests/webp_tizen.c para WASM e serve com COOP/COEP.
#
# COOP/COEP NAO SAO OPCIONAIS: sem "Cross-Origin-Opener-Policy: same-origin" e
# "Cross-Origin-Embedder-Policy: require-corp" o navegador nao entrega
# SharedArrayBuffer, o emscripten aborta o pthread e o teste nao chega a rodar.
# Na TV isso nao aparece porque o .wgt roda de file:// com origem propria.
#
# NAO E TESTE DE LINHA DE COMANDO: precisa de um navegador de verdade
# (createImageBitmap, canvas 2D). O script compila e SERVE; abra a URL e leia o
# console. Passou quando aparecem, nesta ordem:
#
#   [webp] navegador decodificou o primeiro: 1477x980
#   ok  webp 1477x980 formato=ABGR8888
#       pixel central rgba=229,9,19,255
#   webp: tudo ok
#
# Node nao serve: nao tem createImageBitmap nem canvas.
set -e
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

SAIDA=${NUVIO_SAIDA:-build/teste-webp}
mkdir -p "$SAIDA"
echo "isto nao e webp" > "$SAIDA/nao-e-webp.txt"

emcc tests/webp_tizen.c src/webp.c -o "$SAIDA/index.html" -O1 \
  -sUSE_SDL=2 -sWASM_BIGINT=0 \
  -pthread -sPTHREAD_POOL_SIZE=2 \
  -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 \
  -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  -sEXIT_RUNTIME=0 \
  --preload-file tests/amostra.webp@/amostra.webp \
  --preload-file "$SAIDA/nao-e-webp.txt"@/nao-e-webp.txt

PORTA=${NUVIO_PORTA:-8791}
echo "webp-tizen.sh: http://127.0.0.1:$PORTA/  (Ctrl-C para parar)"
exec python3 - "$SAIDA" "$PORTA" <<'PY'
import functools, http.server, socketserver, sys
raiz, porta = sys.argv[1], int(sys.argv[2])
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header('Cross-Origin-Opener-Policy', 'same-origin')
        self.send_header('Cross-Origin-Embedder-Policy', 'require-corp')
        self.send_header('Cache-Control', 'no-store')
        super().end_headers()
    def log_message(self, *a): pass
socketserver.TCPServer.allow_reuse_address = True
with socketserver.TCPServer(('127.0.0.1', porta), functools.partial(H, directory=raiz)) as s:
    s.serve_forever()
PY
