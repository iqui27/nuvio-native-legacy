#!/bin/bash
# Resolver de producao com o motor de playbooks do Farol (CORRECAO-FAROL6B/6C).
# Compila src/resolver.c, a fachada src/farol.c (issue 19), src/extplayer.c (Android), src/addonguia.c,
# src/addonmodo.c e os trechos reais de src/player.c e src/app.c.
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
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-resolver-test.XXXXXX")
trap 'rm -rf "$work"' EXIT

python3 tests/farol_resolver_ponte.py "$work"

"$ccbin" ${flags[@]+"${flags[@]}"} -DRESOLVER_TESTE -DNV_ANDROID -DNV_SHOT_HOOKS \
  -Wall -Wextra -O1 -g \
  tests/farol_resolver.c src/resolver.c src/resolverregra.c src/farolregra.c src/farol.c \
  src/farolerro.c src/farolanel.c src/fontesonda.c src/extplayer.c \
  src/addonguia.c src/addonmodo.c src/farolacao_addon.c src/addonstats.c \
  -I"$work" -Isrc $(pkg-config --cflags sdl2 2>/dev/null || echo -I/usr/include/SDL2) \
  $(pkg-config --libs sdl2 2>/dev/null || echo -lSDL2) -lm -lpthread \
  -Wl,--wrap=SDL_GetTicks \
  -o "$work/farol-resolver-test"

if [ "${SANITIZE:-1}" = thread ] && command -v setarch >/dev/null 2>&1 &&
   setarch "$(uname -m)" -R true 2>/dev/null; then
  setarch "$(uname -m)" -R "$work/farol-resolver-test"
else
  "$work/farol-resolver-test"
fi
echo "farol_resolver: tudo ok"
