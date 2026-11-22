#!/bin/bash
# Cena e validador do Farol (lote sense, issue 39). Ver tests/farol_peca.c.
#   bash tests/farol_peca.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_peca.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-peca-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_peca.c src/farolpeca.c src/faroltexto.c src/js.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"
# Aridade de modelo ("%%" nao conta marcador): inclui o .c e alcanca a
# funcao estatica fp_modelo_args (tests/farol_pct.c).
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_pct.c src/faroltexto.c src/js.c -Isrc -o "$bin-pct"
"$bin-pct"
rm -f "$bin-pct"
# O validador e estatico: malloc na implementacao e bug de contrato, nao de
# teste. E nenhum printf/titulo em log novo nestes modulos (PLANO-GOSTO 6).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolpeca.c; then
  echo "farol_peca: FALHA (malloc/free em src/farolpeca.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log)[[:space:]]*\(' src/farolpeca.c; then
  echo "farol_peca: FALHA (log em src/farolpeca.c)" >&2
  exit 1
fi
echo "farol_peca: tudo ok"
