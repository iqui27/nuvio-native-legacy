#!/bin/bash
# Compositor deterministico do FarolCena (lote sense, issue 44). Ver
# tests/farol_cena.c: linka o compositor + motor + modelo + fatos +
# validador, sem SDL, sem rede, sem dados.h.
#   bash tests/farol_cena.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_cena.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-cena-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_cena.c src/farolcena.c src/farolrec.c src/farolgosto.c \
  src/farolinfo.c src/farolpeca.c src/faroltexto.c src/js.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"
# Contrato do modulo: sem malloc, sem log e sem frase de gosto no .c —
# os motivos saem estruturados do motor e as falas vem de faroltexto.h.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolcena.c; then
  echo "farol_cena: FALHA (malloc/free em src/farolcena.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log|LOGD)[[:space:]]*\(' src/farolcena.c; then
  echo "farol_cena: FALHA (log em src/farolcena.c)" >&2
  exit 1
fi
# Puro como os irmaos: nada de SDL, rede, dados.h ou relogio proprio.
if grep -nE '#include[[:space:]]*[<"](SDL|curl|dados\.h|unistd\.h)' src/farolcena.c src/farolcena.h; then
  echo "farol_cena: FALHA (include proibido em farolcena)" >&2
  exit 1
fi
if grep -nE '\b(time|localtime|gmtime|mktime|clock_gettime|gettimeofday|fopen|open|socket|connect)[[:space:]]*\(' src/farolcena.c; then
  echo "farol_cena: FALHA (relogio ou I/O em src/farolcena.c)" >&2
  exit 1
fi
echo "farol_cena: tudo ok"
