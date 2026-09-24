#!/bin/bash
# Compila e roda tests/fio1.c em Node: testa o escalonador de fibras de
# src/fio1.c isolado do resto do app (produtor/consumidor com mutex+cond,
# SDL_CreateThread/SDL_Delay, join de detached, cond_timedwait, polling
# assincrono no estilo do nv_http de src/rede.c). Nao depende de navegador
# nem de TV — so precisa do Node que o proprio emsdk ja usa para os testes
# do compilador.
#
# AS MESMAS FLAGS DE ESCALONADOR do bloco UM_FIO de tools/tizen.sh (ASYNCIFY,
# ASYNCIFY_STACK_SIZE, os --wrap): se as duas listas de --wrap divergirem,
# este teste passa com uma lista e o app real linka com outra — por isso a
# lista abaixo e comentada como "espelha tools/tizen.sh", nao reinventada.
set -e
cd "$(dirname "$0")/.."

: "${EMSDK_DIR:=$HOME/emsdk}"
[ -f "$EMSDK_DIR/emsdk_env.sh" ] || {
  echo "tests/fio1.sh: emsdk nao encontrado em $EMSDK_DIR" >&2
  exit 2
}
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

SAIDA="${NUVIO_FIO1_TEST_SAIDA:-build/tests-fio1}"
mkdir -p "$SAIDA"

# ESPELHA O BLOCO UM_FIO=1 DE tools/tizen.sh (--wrap e ASYNCIFY_STACK_SIZE).
# Nao inclui -DNV_VIDAA nem as flags de grafico/arte: este teste nao usa nada
# do resto do app, so pthread.h/SDL.h e src/fio1.c.
emcc tests/fio1.c src/fio1.c -o "$SAIDA/fio1.js" \
  -I src \
  -DNV_UM_FIO=1 \
  -sUSE_SDL=2 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 \
  -sEXIT_RUNTIME=1 -sASSERTIONS="${NUVIO_ASSERTS:-1}" \
  -Wl,--wrap=pthread_create -Wl,--wrap=pthread_join -Wl,--wrap=pthread_detach \
  -Wl,--wrap=pthread_self -Wl,--wrap=pthread_equal \
  -Wl,--wrap=pthread_mutex_init -Wl,--wrap=pthread_mutex_destroy \
  -Wl,--wrap=pthread_mutex_lock -Wl,--wrap=pthread_mutex_trylock -Wl,--wrap=pthread_mutex_unlock \
  -Wl,--wrap=pthread_cond_init -Wl,--wrap=pthread_cond_destroy \
  -Wl,--wrap=pthread_cond_signal -Wl,--wrap=pthread_cond_broadcast \
  -Wl,--wrap=pthread_cond_wait -Wl,--wrap=pthread_cond_timedwait \
  -Wl,--wrap=usleep -Wl,--wrap=nanosleep \
  -Wl,--wrap=SDL_CreateThread -Wl,--wrap=SDL_WaitThread -Wl,--wrap=SDL_DetachThread \
  -Wl,--wrap=SDL_CreateMutex -Wl,--wrap=SDL_LockMutex -Wl,--wrap=SDL_TryLockMutex \
  -Wl,--wrap=SDL_UnlockMutex -Wl,--wrap=SDL_DestroyMutex \
  -Wl,--wrap=SDL_CreateCond -Wl,--wrap=SDL_DestroyCond \
  -Wl,--wrap=SDL_CondSignal -Wl,--wrap=SDL_CondBroadcast -Wl,--wrap=SDL_CondWait \
  -Wl,--wrap=SDL_Delay

node "$SAIDA/fio1.js" | tee "$SAIDA/saida.txt"
if grep -q '^FAIL' "$SAIDA/saida.txt"; then
  echo "tests/fio1.sh: alguma verificacao FAIL acima" >&2
  exit 1
fi
if ! grep -q '^=== OK ' "$SAIDA/saida.txt"; then
  echo "tests/fio1.sh: saida nao terminou em '=== OK' (abort ou crash antes do fim?)" >&2
  exit 1
fi
echo "tests/fio1.sh: todas as verificacoes passaram"
