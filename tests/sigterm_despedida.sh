#!/bin/bash
# SIGTERM tem de ser uma saida LIMPA: a sessao seguinte nao pode dizer "nao se
# despediu" nem contar queda rapida no modo seguro (src/seguro.c).
#
#   bash tests/sigterm_despedida.sh
#
# Roda o app do Mac de verdade, numa pasta de dados TEMPORARIA (vazia: nao
# toca o ~/.nuvio nem rotaciona o token de ninguem), manda SIGTERM so no
# processo que ele mesmo iniciou, sobe de novo e le o log da segunda sessao.
# Nunca aperta Play: a tela de login basta, avisos e seguro iniciam antes dela.
set -u
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
mkdir -p "$TMPDIR"
W=$(mktemp -d "$TMPDIR/nv-sigterm.XXXXXX")
BIN="$W/app"
ENV_D=$(tools/env.sh)
eval cc src/*.c src/dts/*.c -o "$BIN" -O1 -g "$ENV_D" \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz \
  -framework OpenGL -Wno-deprecated-declarations || { echo "FALHOU: compilacao"; exit 2; }
DADOS="$W/dados"; mkdir -p "$DADOS"
PID=""
trap '[ -n "$PID" ] && kill -KILL "$PID" 2>/dev/null; rm -rf "$W"' EXIT

sessao() { # numero do log; mata com SIGTERM depois de a abertura terminar
  local log="$W/log$1.txt" i
  NUVIO_DADOS="$DADOS" "$BIN" "$PWD/deploy/app/art" >"$log" 2>&1 &
  PID=$!
  for i in $(seq 1 120); do grep -q "abertura\] fim" "$log" 2>/dev/null && break; sleep 0.25; done
  sleep 1
  [ "$1" = 1 ] && { kill -TERM "$PID"; for i in $(seq 1 40); do kill -0 "$PID" 2>/dev/null || break; sleep 0.25; done; }
  [ "$1" = 2 ] && { kill -TERM "$PID"; wait "$PID" 2>/dev/null; }
  wait "$PID" 2>/dev/null; PID=""
}
sessao 1
sessao 2
fail=0
if grep -q "nao se despediu\|anterior caiu" "$W/log2.txt"; then
  echo "FALHOU: a segunda sessao achou que a primeira caiu:"; grep -n "nao se despediu\|anterior caiu\|\[seguro\]" "$W/log2.txt"; fail=1
fi
grep -q "\[seguro\].*anterior saiu limpo" "$W/log2.txt" || { echo "FALHOU: seguro nao viu saida limpa"; grep -n "\[seguro\]" "$W/log2.txt"; fail=1; }
grep -q "quedas rapidas [1-9]" "$W/log2.txt" && { echo "FALHOU: queda rapida contada"; fail=1; }
[ $fail = 0 ] && echo "OK: SIGTERM encerra limpo (sem 'nao se despediu', sem queda rapida)"
exit $fail
