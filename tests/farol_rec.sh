#!/bin/bash
# Motor de recomendacao do Farol (lote sense, issue 41; PLANO-GOSTO secao 3).
# Ver tests/farol_rec.c. Linka so farolrec.c + farolgosto.c: sem SDL, sem
# rede, sem dados.h — o relogio e injetado pelo contexto do motor.
#   bash tests/farol_rec.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=0 bash tests/farol_rec.sh       (sem sanitizador: so os asserts)
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  0) ;;
  *) flags+=(-fsanitize="$SANITIZE") ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-rec-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O2 -g \
  tests/farol_rec.c src/farolrec.c src/farolgosto.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"
# Contrato do modulo: sem malloc e sem log — muito menos com titulo (o motor
# nao fala; os motivos saem estruturados, PLANO-GOSTO 6 e 3.5).
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolrec.c; then
  echo "farol_rec: FALHA (malloc/free em src/farolrec.c)" >&2
  exit 1
fi
if grep -nE '\b(printf|fprintf|syslog|NSLog|__android_log|LOGD)[[:space:]]*\(' src/farolrec.c; then
  echo "farol_rec: FALHA (log em src/farolrec.c)" >&2
  exit 1
fi
# O motor nao carrega frase nenhuma: literal de fala no .c e texto solto.
if grep -nE '"(Porque|Voc|gosta|Indicado|Fora do)' src/farolrec.c; then
  echo "farol_rec: FALHA (frase de motivo em src/farolrec.c)" >&2
  exit 1
fi
echo "farol_rec: tudo ok"
