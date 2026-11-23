#!/bin/bash
# Aquecimento, backup e notas antigas do Trakt (lote sense, issue 43;
# PLANO-GOSTO 1.3 e 2.3). Ver tests/farol_aquece.c — reusa os falsos do
# farol_sinal.c por inclusao; o gosto, a ficha, o anel, o mapa e o js sao os
# reais (o mapa vai em -DNV_MAPA_PURO como no teste da ficha).
#   bash tests/farol_aquece.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_aquece.sh       (sem sanitizador)
#   SANITIZE=thread bash tests/farol_aquece.sh  (TSan, com setarch -R)
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
bin="${TMPDIR:-/tmp}/nuvio-farol-aquece-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -DNV_MAPA_PURO \
  -Isrc tests/farol_aquece.c src/farolsinal.c src/farolgosto.c \
  src/farolficha.c src/farolanel.c src/mapa.c src/js.c -lpthread -lm -o "$bin"
$prefix "$bin"
rm -f "$bin"
echo "farol_aquece: tudo ok"
