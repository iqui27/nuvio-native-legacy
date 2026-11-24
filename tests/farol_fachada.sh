#!/bin/bash
# Fachada de diagnostico do Farol (issue #14, Onda 2, ADENDO 2.1 C).
# Compila src/farol.c, src/farolanel.c, src/farolregra.c, src/resolverregra.c e
# src/farolerro.c sem SDL2 e valida invariantes estaticas e funcionais.
#
#   bash tests/farol_fachada.sh
#   SANITIZE=0 bash tests/farol_fachada.sh
set -eu
cd "$(dirname "$0")/.."

# Invariantes estaticas dos contratos do Farol #14. `! grep` em set -e nao
# mata o script (a negacao anula o exit), entao cada checagem e explicita
# (revisao FAROL14 r1).
if grep -n "_Thread_local" src/farol.c src/farol.h src/farolanel.c src/farolanel.h; then
  echo "farol_fachada: _Thread_local proibido nos modulos do Farol" >&2; exit 1;
fi
if grep -n "SDL2/SDL" src/farol.c src/farol.h; then
  echo "farol_fachada: SDL2 nao pode entrar em src/farol.c/.h" >&2; exit 1;
fi
if grep -n "NV_DTS_FFMPEG" src/*.h; then
  echo "farol_fachada: NV_DTS_FFMPEG nao pode aparecer em headers" >&2; exit 1;
fi

flags=(-Wall -Wextra -O1 -g -DRESOLVER_TESTE)
ccbin="${CC:-cc}"
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-fachada.XXXXXX")
trap 'rm -rf "$work"' EXIT

srcs=(
  tests/farol_fachada.c
  src/farol.c
  src/farolanel.c
  src/farolregra.c
  src/resolverregra.c
  src/farolerro.c
  src/fontesonda.c
)

"$ccbin" "${flags[@]}" "${srcs[@]}" -Isrc -lm -lpthread -o "$work/farol-fachada"
"$work/farol-fachada"

"$ccbin" "${flags[@]}" -DNV_ANDROID "${srcs[@]}" -Isrc -lm -lpthread -o "$work/farol-fachada-android"
"$work/farol-fachada-android"

echo "farol_fachada: tudo ok"
