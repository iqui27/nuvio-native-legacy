#!/bin/bash
# Descoberta local pura do Farol (PLANO secao 2.9, issue 24).
# Ver tests/farol_acha.c. Linka so src/farolacha.c: sem SDL, sem rede, sem dados.h.
#   bash tests/farol_acha.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_acha.sh       (sem sanitizador)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  *) flags+=(-fsanitize="$SANITIZE") ;;
esac
workdir="${TMPDIR:-tests/out}"
mkdir -p "$workdir"
bin=$(mktemp "$workdir/nuvio-farol-acha.XXXXXX")
trap 'rm -f "$bin"; rmdir tests/out 2>/dev/null || true' EXIT
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_acha.c src/farolacha.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"

# Contrato de pureza: sem malloc, sem SDL/rede/dados.h, sem log e sem estado
# estatico mutavel (nenhum indice ou ponteiro guardado entre quadros).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolacha.c; then
  echo "farol_acha: FALHA (malloc/free em src/farolacha.c)" >&2
  exit 1
fi
if grep -nE '#[[:space:]]*include[[:space:]]*[<"](SDL|dados\.h|rede\.h|curl)' src/farolacha.c src/farolacha.h; then
  echo "farol_acha: FALHA (dependencia proibida em farolacha)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log|LOGD|puts|fputs)[[:space:]]*\(' src/farolacha.c; then
  echo "farol_acha: FALHA (log em src/farolacha.c)" >&2
  exit 1
fi
if grep -nE '\b(rand|srand|random|time|gettimeofday|clock_gettime)[[:space:]]*\(' src/farolacha.c; then
  echo "farol_acha: FALHA (nao determinismo em src/farolacha.c)" >&2
  exit 1
fi
echo "farol_acha: tudo ok"
