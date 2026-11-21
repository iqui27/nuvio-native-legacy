#!/bin/bash
# Fatos da informacao relevante, peca P6 (lote sense, issue 47).
# Ver tests/farol_info.c — tabela de casos, um por regra do aceite.
#   bash tests/farol_info.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_info.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-info-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_info.c src/farolinfo.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"

# O contrato de pureza: sem malloc, sem SDL, sem rede, sem dados.h, sem
# time() — o relogio chega injetado — e sem log (nenhum titulo em log novo).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolinfo.c; then
  echo "farol_info: FALHA (malloc/free em src/farolinfo.c)" >&2
  exit 1
fi
if grep -nE '#[[:space:]]*include[[:space:]]*[<"](SDL|dados\.h|rede\.h|curl)' src/farolinfo.c src/farolinfo.h; then
  echo "farol_info: FALHA (dependencia proibida em farolinfo)" >&2
  exit 1
fi
if grep -nE '\b(time|localtime|localtime_r|gmtime|strftime|gettimeofday|clock_gettime)[[:space:]]*\(' src/farolinfo.c; then
  echo "farol_info: FALHA (relogio lido no modulo; deve ser injetado)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|LOGD|puts|fputs)\(' src/farolinfo.c; then
  echo "farol_info: FALHA (log em src/farolinfo.c)" >&2
  exit 1
fi
# Sem texto livre: o modulo so cita chaves FT_*, nunca literal de fala.
if grep -nE '"[^"]*[A-Za-z][^"]* (voc|sua|TV|est|Tem|Falta|Dá|termin)' src/farolinfo.c; then
  echo "farol_info: FALHA (literal de fala no modulo; frases sao da 35)" >&2
  exit 1
fi
# Frase livre fora do vocabulario: literal que FECHA como frase (pontuacao
# final) em linha de codigo. Comentarios FALTA_TEXTO citam exemplos com
# ponto e ficam de fora.
if grep -nE '"[^"]*[A-Za-z][.!?]"' src/farolinfo.c | grep -vE '^[0-9]+:[[:space:]]*//'; then
  echo "farol_info: FALHA (frase livre no modulo; frases sao da 35)" >&2
  exit 1
fi
echo "farol_info: tudo ok"
