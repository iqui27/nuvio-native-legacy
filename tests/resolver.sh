#!/bin/bash
# Regra do Resolver: ordem dos testes e guarda de 30 dias. Ver tests/resolver.c.
set -eu
cd "$(dirname "$0")/.."
ccbin="${CC:-cc}"
flags=(-Wall -Wextra -O1 -g)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-resolver-test.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$ccbin" "${flags[@]}" tests/resolver.c src/resolverregra.c -Isrc -o "$work/resolver-test"
"$work/resolver-test"
echo "resolver: tudo ok"
