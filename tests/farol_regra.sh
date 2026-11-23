#!/bin/bash
# Motor de playbooks do Farol (lote sense, issue 6). Ver tests/farol_regra.c.
set -eu
cd "$(dirname "$0")/.."
flags=()
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
bin="${TMPDIR:-/tmp}/nuvio-farol-regra-test.$$"
trap 'rm -f "$bin"' EXIT
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_regra.c src/farolregra.c src/resolverregra.c src/farolerro.c src/fontesonda.c \
  -Isrc -o "$bin"
if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$bin"
else
  "$bin"
fi
rm -f "$bin"
if grep -nE '#include[[:space:]]*[<"](SDL|rede\.h|dados\.h)' src/farolregra.c src/farolregra.h; then
  echo "farol_regra: FALHA (dependencia impura em farolregra)" >&2
  exit 1
fi
if grep -nE '(malloc|calloc|realloc|free)[[:space:]]*\(' src/farolregra.c; then
  echo "farol_regra: FALHA (alocacao dinamica em farolregra.c)" >&2
  exit 1
fi
echo "farol_regra: tudo ok"
