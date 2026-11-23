#!/bin/bash
# Ficha do titulo do Farol (lote sense, issue 36; PLANO-GOSTO secao 2.1).
# Ver tests/farol_ficha.c. Le o MESMO parser do mapa (-DNV_MAPA_PURO compila
# so a parte pura de mapa.c) mais js.c: sem SDL, sem rede, sem dados.h.
#   bash tests/farol_ficha.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_ficha.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  *) flags+=(-fsanitize="$SANITIZE") ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-ficha-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -DNV_MAPA_PURO \
  -Isrc tests/farol_ficha.c src/farolficha.c src/mapa.c src/farolgosto.c src/js.c -lm -o "$bin"
"$bin"
rm -f "$bin"
# Contrato do modulo: sem malloc e sem log — muito menos com titulo.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolficha.c; then
  echo "farol_ficha: FALHA (malloc/free em src/farolficha.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|LOGD)[[:space:]]*\(' src/farolficha.c; then
  echo "farol_ficha: FALHA (log em src/farolficha.c)" >&2
  exit 1
fi
echo "farol_ficha: tudo ok"
