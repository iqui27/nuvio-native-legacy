#!/bin/bash
# Resolver de producao com o motor de playbooks do Farol (CORRECAO-FAROL6B).
# Compila o src/resolver.c real com stubs de app — ver tests/farol_resolver.c.
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
bin="${TMPDIR:-/tmp}/nuvio-farol-resolver-test.$$"
trap 'rm -f "$bin"' EXIT
"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -Wall -Wextra -O1 -g \
  tests/farol_resolver.c src/resolver.c src/resolverregra.c src/farolregra.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c \
  -Isrc $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lpthread -o "$bin"
if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$bin"
else
  "$bin"
fi
rm -f "$bin"
echo "farol_resolver: tudo ok"
