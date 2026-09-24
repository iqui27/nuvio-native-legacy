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
#   [webp] navegador decodificou o primeiro: 1280x720 (image/png), no worker
#   ok  png 4K reduzido 1280x720 (arquivo 3840x2160, pixel=220,30,40,255, ms=...)
#   ok  webp 1477x980 formato=ABGR8888
#       pixel central rgba=229,9,19,255
#   ok  webp reduzido 320x212 (arquivo 1477x980)
#   webp: tudo ok
#   ok  jpeg 320x180 (arquivo 640x360)          <- libjpeg em software, escalado
#   ok  jpeg inteiro 640x360
#   ok  gif primeiro quadro 8x4 (arquivo 8x4, pixel=255,0,0,255)
#   jpeg: tudo ok
#   ok  png pequeno local 160x160 (arquivo 160x160, pedido 64) <- <= 256: libpng local
#   ok  png inteiro 160x160
#   png: tudo ok
#   ok  png transparente /icone-home.png ...     <- alpha 0 e 255 preservadas
#   ok  png transparente /icone-guide.png ...
#   ok  png transparente /icone-search.png ...
#
# Para provar o fallback, repita em outra porta com
# NUVIO_NO_DECODER_WORKER=1; a primeira linha passa a terminar em
# `image/png, no fio principal`, mantendo as mesmas dimensoes e pixel.
#
# Node nao serve: nao tem createImageBitmap nem canvas.
set -e
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
# emsdk_env.sh pode deixar EMSDK definido sem acrescentar o binario ao PATH.
# O harness aceita essa instalacao e torna o caminho de emcc explicito.
export PATH="$EMSDK_DIR/upstream/emscripten:$PATH"
command -v emcc >/dev/null || { echo "webp-tizen.sh: emcc nao encontrado em $EMSDK_DIR/upstream/emscripten" >&2; exit 127; }

SAIDA=${NUVIO_SAIDA:-build/teste-webp}
mkdir -p "$SAIDA"
python3 tests/png_fixture.py "$SAIDA/amostra-4k.png"
echo "isto nao e webp" > "$SAIDA/nao-e-webp.txt"
# GIF de dois quadros, 8x4: o primeiro vermelho, o segundo azul. O que a ponte
# tem de devolver e o PRIMEIRO quadro (a foto de perfil .gif parada).
python3 - "$SAIDA/amostra.gif" <<'PY'
import struct, sys
W, H = 8, 4
def lzw(indice):
    # min code size 2: clear=4, fim=5, largura 3 bits. Um clear antes de cada
    # pixel impede o dicionario de crescer, entao a largura nunca muda.
    codigos = []
    for _ in range(W * H): codigos += [4, indice]
    codigos.append(5)
    bits = n = 0; saida = bytearray()
    for c in codigos:
        bits |= c << n; n += 3
        while n >= 8: saida.append(bits & 255); bits >>= 8; n -= 8
    if n: saida.append(bits & 255)
    return bytes([2]) + bytes([len(saida)]) + bytes(saida) + b'\0'
g = b'GIF89a' + struct.pack('<HHBBB', W, H, 0x81, 0, 0)
g += bytes([255, 0, 0,  0, 0, 255,  0, 0, 0,  0, 0, 0])
g += b'\x21\xff\x0bNETSCAPE2.0\x03\x01\x00\x00\x00'
for i in (0, 1):
    g += b'\x21\xf9\x04\x04' + struct.pack('<H', 10) + b'\x00\x00'
    g += b'\x2c' + struct.pack('<HHHHB', 0, 0, W, H, 0) + lzw(i)
g += b'\x3b'
open(sys.argv[1], 'wb').write(g)
PY

emcc tests/webp_tizen.c src/webp.c src/jpegrapido.c -o "$SAIDA/index.html" -O1 \
  -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png","jpg"]' -sUSE_LIBJPEG=1 -sWASM_BIGINT=0 \
  -pthread -sPTHREAD_POOL_SIZE=2 \
  -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 \
  -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  -sEXIT_RUNTIME=0 \
  --preload-file tests/amostra.webp@/amostra.webp \
  --preload-file tests/amostra.jpg@/amostra.jpg \
  --preload-file tests/fixtures/logos/comfundo.png@/amostra.png \
  --preload-file deploy/app/art/icones/menu_home.png@/icone-home.png \
  --preload-file deploy/app/art/icones/menu_guide.png@/icone-guide.png \
  --preload-file deploy/app/art/icones/menu_search.png@/icone-search.png \
  --preload-file "$SAIDA/amostra-4k.png"@/amostra-4k.png \
  --preload-file "$SAIDA/nao-e-webp.txt"@/nao-e-webp.txt \
  --preload-file "$SAIDA/amostra.gif"@/amostra.gif

cp tools/decodificador.js "$SAIDA/decodificador.js"
if [ "${NUVIO_NO_DECODER_WORKER:-0}" = 1 ]; then
  rm -f "$SAIDA/decodificador.js"
  echo "webp-tizen.sh: worker desabilitado; navegador deve cair no fio principal"
fi

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
