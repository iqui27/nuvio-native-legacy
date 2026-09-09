#!/bin/bash
# Roda o player do alvo Tizen contra um DUBLE de webapis.avplay e imprime a
# sequencia de chamadas. Ver o cabecalho de tools/teste-avplay.c para o que isto
# prova e o que nao prova.
set -e
cd "$(dirname "$0")/.."
[ -n "$EMSDK" ] || source ~/emsdk/emsdk_env.sh >/dev/null 2>&1
SAIDA=build/teste-av
LOG=${NUVIO_LOG:-/tmp/nvteste.txt}
mkdir -p "$SAIDA"
ENV_D=$(tools/env.sh)

# tools/teste-avplay.c entra AQUI e so aqui. O duble vai por --pre-js, que roda
# antes do main e portanto antes de video_iniciar procurar webapis.avplay.
eval emcc src/*.c tools/teste-avplay.c -o "$SAIDA/index.html" -O1 -g2 "$ENV_D" \
  -sUSE_SDL=2 -sUSE_SDL_IMAGE=2 -sUSE_SDL_TTF=2 -sSDL2_IMAGE_FORMATS='["png","jpg"]' \
  -sMAX_WEBGL_VERSION=1 -sINITIAL_MEMORY=402653184 \
  -sSTACK_SIZE=8388608 -sDEFAULT_PTHREAD_STACK_SIZE=8388608 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=32768 \
  -pthread -sPTHREAD_POOL_SIZE=32 -sPTHREAD_POOL_SIZE_STRICT=0 \
  -sEXPORTED_FUNCTIONS='["_main","_malloc","_free","_nv_teste_avplay","_nv_teste_avplay_fase2","_nv_teste_avplay_zoom","_nv_teste_avplay_fim","_nv_teste_bombear"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall"]' \
  -lidbfs.js -sEXIT_RUNTIME=0 \
  --preload-file deploy/app/fonts@/app/fonts \
  --shell-file tools/tizen-shell.html \
  --pre-js tools/fake-avplay.js \
  --pre-js "${NUVIO_PREJS:-/dev/null}"

echo "teste-avplay.sh: build em $SAIDA"
