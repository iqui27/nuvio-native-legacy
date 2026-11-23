#!/bin/bash
# Anel de eventos do Farol (lote sense, issue 2). Ver tests/farol_anel.c.
#   bash tests/farol_anel.sh                  (ASan+UBSan, o padrao)
#   SANITIZE=thread bash tests/farol_anel.sh  (TSan: os 4 fios do teste)
#   SANITIZE=0 bash tests/farol_anel.sh       (sem sanitizador: so os asserts)
# O modulo e de fios por contrato (produtores gravam de qualquer fio), entao
# entra tambem na lista TSan de tools/sanitiza.sh.
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    # O TSan do gcc 13 morre no arranque nesta imagem ("unexpected memory
    # mapping", ate num int main(void){return 0;}); o do clang funciona.
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-anel-test"
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g -pthread \
  tests/farol_anel.c src/farolanel.c -Isrc -o "$bin"
"$bin"
rm -f "$bin"
# O anel e estatico: malloc na implementacao e bug de contrato, nao de teste.
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolanel.c; then
  echo "farol_anel: FALHA (malloc/free em src/farolanel.c)" >&2
  exit 1
fi
echo "farol_anel: tudo ok"
