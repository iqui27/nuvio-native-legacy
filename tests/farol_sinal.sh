#!/bin/bash
# Sinais do Farol (lote sense, issue 38; PLANO-GOSTO 1.3 e 2.2). Ver
# tests/farol_sinal.c — a tabela 2.2 linha a linha, as exclusoes, o desfazer
# e as tres ressalvas do lote, tudo contra o farolsinal.c de producao com
# diario/anel/catalogo falsos por baixo (o anel, a ficha, o gosto, o mapa e o
# js sao os reais; o mapa vai em -DNV_MAPA_PURO como no teste da ficha).
#   bash tests/farol_sinal.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_sinal.sh       (sem sanitizador)
#   SANITIZE=thread bash tests/farol_sinal.sh  (TSan, com setarch -R)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
prefix=""
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  thread) flags+=(-fsanitize=thread)
          prefix="setarch -R" ;;
  *) flags+=(-fsanitize="$SANITIZE") ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-sinal-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -DNV_MAPA_PURO \
  -Isrc tests/farol_sinal.c src/farolsinal.c src/farolgosto.c \
  src/farolficha.c src/farolanel.c src/mapa.c src/js.c -lpthread -lm -o "$bin"
$prefix "$bin"
rm -f "$bin"
# O modulo grava estado com fprintf(f, ...) — persistencia, nao log. O que o
# ALMA.md proibe e log: printf (stdout), fprintf(stderr/stdout) e syslog.
if grep -nE '\bprintf[[:space:]]*\(|fprintf[[:space:]]*\([[:space:]]*(stderr|stdout)|\b(syslog|NSLog|LOGD|__android_log)[[:space:]]*\(' src/farolsinal.c; then
  echo "farol_sinal: FALHA (log em src/farolsinal.c)" >&2
  exit 1
fi
echo "farol_sinal: tudo ok"
