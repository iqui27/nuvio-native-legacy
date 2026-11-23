#!/bin/bash
# Modelo de gosto do Farol (lote sense, issue 37). Ver tests/farol_gosto.c.
#   bash tests/farol_gosto.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_gosto.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-gosto-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_gosto.c src/farolgosto.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"
# O modelo e estatico: malloc na implementacao e bug de contrato, nao de
# teste. E nenhum printf/titulo em log novo nestes modulos (PLANO-GOSTO 6).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolgosto.c; then
  echo "farol_gosto: FALHA (malloc/free em src/farolgosto.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log)[[:space:]]*\(' src/farolgosto.c; then
  echo "farol_gosto: FALHA (log em src/farolgosto.c)" >&2
  exit 1
fi
echo "farol_gosto: tudo ok"
