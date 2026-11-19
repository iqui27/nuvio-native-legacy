#!/bin/bash
# Compila tests/webp_vidaa_st.c no modo VIDAA st/ (--um-fio) e serve SEM
# COOP/COEP — a condicao da pagina VIDAA hospedada, sem SharedArrayBuffer.
#
# NAO E TESTE DE LINHA DE COMANDO, pelo mesmo motivo de tests/webp-tizen.sh:
# precisa de createImageBitmap e canvas. Abra a URL e leia o console. Passou
# quando aparecem as mesmas linhas "ok" de tests/webp-tizen.sh, com a primeira
# terminando em `image/png, no fio principal` (nao ha Worker sem memoria
# compartilhada), e no fim:
#
#   ok  webp com fio principal ocupado: 10/10 ok, media=... ms
#   fim do teste
#
# A media ali e de segundos, e isso e esperado: com um fio so, cada bloco de
# 1 s do fio principal ocupado segura tambem o decode.
#
# AS FLAGS DE FIO ESPELHAM O BLOCO UM_FIO DE tools/tizen.sh: os --wrap sao
# lidos de la, nao copiados, para as duas listas nao divergirem (a de
# tests/fio1.sh nao tem o emscripten_async_run_in_main_runtime_thread_ que o
# webp.c precisa para linkar).
set -e
cd "$(dirname "$0")/.."
: "${EMSDK_DIR:=$HOME/emsdk}"
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
export PATH="$EMSDK_DIR/upstream/emscripten:$PATH"
command -v emcc >/dev/null || { echo "webp-vidaa-st.sh: emcc nao encontrado em $EMSDK_DIR/upstream/emscripten" >&2; exit 127; }

SAIDA=${NUVIO_SAIDA:-build/teste-webp-st}
mkdir -p "$SAIDA"
python3 tests/png_fixture.py "$SAIDA/amostra-4k.png"
echo "isto nao e webp" > "$SAIDA/nao-e-webp.txt"

WRAPS=$(grep -o -- '--wrap=[A-Za-z_]*' tools/tizen.sh | sort -u | sed 's/^/-Wl,/')
# shellcheck disable=SC2086
emcc tests/webp_vidaa_st.c src/webp.c src/jpegrapido.c src/fio1.c -I src -o "$SAIDA/index.html" -O1 \
  -DNV_UM_FIO=1 \
  -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sSDL2_IMAGE_FORMATS='["png","jpg"]' -sUSE_LIBJPEG=1 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 \
  -sINITIAL_MEMORY=134217728 -sALLOW_MEMORY_GROWTH=0 \
  -sEXPORTED_FUNCTIONS='["_main","_malloc","_free"]' \
  -sEXIT_RUNTIME=0 $WRAPS \
  --preload-file tests/amostra.webp@/amostra.webp \
  --preload-file tests/amostra.jpg@/amostra.jpg \
  --preload-file tests/fixtures/logos/comfundo.png@/amostra.png \
  --preload-file deploy/app/art/icones/menu_home.png@/icone-home.png \
  --preload-file deploy/app/art/icones/menu_guide.png@/icone-guide.png \
  --preload-file deploy/app/art/icones/menu_search.png@/icone-search.png \
  --preload-file "$SAIDA/amostra-4k.png"@/amostra-4k.png \
  --preload-file "$SAIDA/nao-e-webp.txt"@/nao-e-webp.txt
cp tools/decodificador.js "$SAIDA/decodificador.js"

PORTA=${NUVIO_PORTA:-8792}
echo "webp-vidaa-st.sh: http://127.0.0.1:$PORTA/  (Ctrl-C para parar)"
exec python3 -m http.server "$PORTA" --bind 127.0.0.1 --directory "$SAIDA"
