#!/bin/bash
# F90 / PR573: roda tests/fonteurl_wasm.c em Node (WASM via emcc, fetch
# simulado pelo --pre-js tests/fonteurl_wasm.js). Verifica o contrato do
# receber_corpo de rede_pedir nos dois motores WGT:
#   * NV_UM_FIO (cooperativo): cada fatia chega ao callback assim que o fetch
#     a le, retorno 0 aborta na hora, e a drenagem de um corpo de 32 MiB nao
#     segura um passo do escalonador alem do teto. Este modo reprova a
#     a6bce7d69, que acumulava o corpo ate o EOF.
#   * pthread padrao (EM_ASYNC_JS): o mesmo contrato, que ja funcionava.
# Nao depende de navegador nem de TV — so do Node que o proprio emsdk usa.
set -e
cd "$(dirname "$0")/.."

: "${EMSDK_DIR:=$HOME/emsdk}"
[ -f "$EMSDK_DIR/emsdk_env.sh" ] || {
  echo "tests/fonteurl_wasm.sh: emsdk nao encontrado em $EMSDK_DIR" >&2
  exit 2
}
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

SAIDA="${NUVIO_FONTEURL_WASM_SAIDA:-build/tests-fonteurl-wasm}"
mkdir -p "$SAIDA"

# Mesmas flags do modo UM_FIO de tools/tizen.sh: a lista de --wrap e lida do
# tests/fio1.sh, que ja a espelha — se divergir la, diverge aqui junto.
mapfile -t wrap < <(grep -oE -- '-Wl,--wrap=[[:alnum:]_]+' tests/fio1.sh)

# Cooperativo (fio1): o caminho corrigido pela F90.
emcc -O1 -Isrc -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=262144 \
  -sINITIAL_MEMORY=134217728 -sASSERTIONS="${NUVIO_ASSERTS:-1}" -sEXIT_RUNTIME=1 \
  --pre-js tests/fonteurl_wasm.js \
  tests/fonteurl_wasm.c src/fonteurl.c src/rede.c src/redeurl.c \
  -D_DEFAULT_SOURCE -DNV_UM_FIO -sUSE_SDL=2 src/fio1.c "${wrap[@]}" \
  -o "$SAIDA/fonteurl_wasm_fio.js"

# WGT padrao com pthread: o EM_ASYNC_JS nv_http_fluxo, que a F90 nao toca.
emcc -O1 -Isrc -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 -sSTACK_SIZE=262144 \
  -sINITIAL_MEMORY=134217728 -sASSERTIONS="${NUVIO_ASSERTS:-1}" -sEXIT_RUNTIME=1 \
  --pre-js tests/fonteurl_wasm.js \
  tests/fonteurl_wasm.c src/fonteurl.c src/rede.c src/redeurl.c \
  -pthread -sPTHREAD_POOL_SIZE=1 \
  -o "$SAIDA/fonteurl_wasm_pthread.js"

for modo in fio pthread; do
  node "$SAIDA/fonteurl_wasm_$modo.js" | tee "$SAIDA/$modo.txt"
  if ! grep -q '^=== OK fonteurl_wasm' "$SAIDA/$modo.txt"; then
    echo "tests/fonteurl_wasm.sh: modo $modo nao terminou em '=== OK'" >&2
    exit 1
  fi
done
echo "tests/fonteurl_wasm.sh: todos os modos passaram"
