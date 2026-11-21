#!/bin/bash
# Corrida da primeira geracao de UUID em instalacao nova (worker de add-ons x
# tela de Diagnostico). Ver tests/uuid_concorrente.c.
#   bash tests/uuid_concorrente.sh                  (sem sanitizador: so os asserts)
#   SANITIZE=1 bash tests/uuid_concorrente.sh       (ASan+UBSan)
#   SANITIZE=thread bash tests/uuid_concorrente.sh  (TSan, o modo da regressao)
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-uuid-concorrente.XXXXXX")
trap 'rm -rf "$dir"' EXIT
ccbin="${CC:-cc}"
flags=(-O1 -g -I. -Isrc -Itests -pthread -Wall -Wextra
       -ffunction-sections -fdata-sections
       $(pkg-config --cflags sdl2 freetype2 libcurl 2>/dev/null || true))
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    # O TSan do gcc morre no arranque nesta imagem; o do clang funciona
    # (mesmo motivo de tests/cliente_corrida.sh).
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
linker=-Wl,--gc-sections
wrap=-Wl,--wrap=srand
if [ "$(uname -s)" = Darwin ]; then
  linker=-Wl,-dead_strip
  # ld64 nao aceita --wrap; o uuid_concorrente.c interpoe srand direto no
  # binario (simbolo srand + RTLD_NEXT) quando __APPLE__.
  wrap=
fi
"$ccbin" "${flags[@]}" "$linker" $wrap \
  tests/uuid_concorrente.c src/addons.c src/addonmodo.c src/fontefalha.c \
  src/farolanel.c src/js.c src/idcanal.c src/watchstate.c src/dados.c \
  -o "$dir/teste"
env FAROL_TESTE_BARREIRA=1 NUVIO_DADOS="$dir/dados" HOME="$dir/dados" \
  TSAN_OPTIONS='halt_on_error=1' "$dir/teste"
echo "uuid_concorrente.sh: ok"
