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
bin="${TMPDIR:-/tmp}/nuvio-farol-regra-test"
trap 'rm -f "$bin"' EXIT
"$ccbin" ${flags[@]+"${flags[@]}"} -Wall -Wextra -O1 -g \
  tests/farol_regra.c src/farolregra.c src/resolverregra.c src/farolerro.c src/fontesonda.c \
  -Isrc -o "$bin"
"$bin"
rm -f "$bin"
echo "farol_regra: tudo ok"
