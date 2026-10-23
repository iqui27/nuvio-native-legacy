#!/bin/bash
# Compila e roda tests/fio1_pilha.c em Node: a pilha C de toda fibra de
# src/fio1.c tem de sair alinhada em 16, e um fwrite grande dentro da fibra
# tem de terminar (com a pilha 8 mod 16 ele girava para sempre — ver o
# cabecalho do .c). Mesmas flags de escalonador de tests/fio1.sh, que
# espelham o bloco UM_FIO de tools/tizen.sh. -O2 de proposito: o defeito
# aparece no codigo otimizado (a libc do emsdk ja vem assim).
set -e
cd "$(dirname "$0")/.."

: "${EMSDK_DIR:=$HOME/emsdk}"
[ -f "$EMSDK_DIR/emsdk_env.sh" ] || {
  echo "tests/fio1-pilha.sh: emsdk nao encontrado em $EMSDK_DIR" >&2
  exit 2
}
# shellcheck disable=SC1091
source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1

SAIDA="${NUVIO_FIO1_TEST_SAIDA:-build/tests-fio1-pilha}"
mkdir -p "$SAIDA"

emcc tests/fio1_pilha.c src/fio1.c -o "$SAIDA/fio1_pilha.js" -O2 \
  -I src \
  -DNV_UM_FIO=1 \
  -sUSE_SDL=2 \
  -sASYNCIFY -sASYNCIFY_STACK_SIZE=131072 \
  -sEXIT_RUNTIME=1 \
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

# PRAZO: com o defeito o fwrite nao volta nunca e o node fica preso — sem
# isto o teste travaria quem o roda, em vez de falhar.
set +e
perl -e 'alarm shift; exec @ARGV' 60 node "$SAIDA/fio1_pilha.js" > "$SAIDA/saida.txt" 2>&1
rc=$?
set -e
cat "$SAIDA/saida.txt"
if [ "$rc" = 142 ]; then
  echo "tests/fio1-pilha.sh: FAIL — nao terminou em 60 s (fwrite preso numa fibra)" >&2
  exit 1
fi
if grep -q '^FAIL' "$SAIDA/saida.txt" || ! grep -q '^=== OK ' "$SAIDA/saida.txt"; then
  echo "tests/fio1-pilha.sh: FAIL" >&2
  exit 1
fi
echo "tests/fio1-pilha.sh: todas as verificacoes passaram"
