#!/bin/bash
# Corrida da primeira inicializacao de cliente.txt (worker de add-ons x fio de
# sync). Ver tests/cliente_corrida.c.
#   bash tests/cliente_corrida.sh                  (sem sanitizador: so os asserts)
#   SANITIZE=1 bash tests/cliente_corrida.sh       (ASan+UBSan)
#   SANITIZE=thread bash tests/cliente_corrida.sh  (TSan, o modo da regressao)
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cliente-corrida.XXXXXX")
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
    # (mesmo motivo de tests/farol_anel.sh).
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac
linker=-Wl,--gc-sections
wrap="-Wl,--wrap=fopen -Wl,--wrap=fopen64"
if [ "$(uname -s)" = Darwin ]; then
  linker=-Wl,-dead_strip
  # ld64 nao aceita --wrap; o cliente_corrida.c interpoe fopen direto no
  # binario (simbolo fopen + RTLD_NEXT) quando __APPLE__.
  wrap=
fi
"$ccbin" "${flags[@]}" "$linker" $wrap \
  tests/cliente_corrida.c src/addons.c src/addonmodo.c src/fontefalha.c \
  src/farolanel.c src/js.c src/idcanal.c src/watchstate.c src/dados.c \
  -o "$dir/teste"
env FAROL_TESTE_BARREIRA=1 NUVIO_DADOS="$dir/dados" HOME="$dir/dados" \
  TSAN_OPTIONS='halt_on_error=1' "$dir/teste"
echo "cliente_corrida.sh: ok"
