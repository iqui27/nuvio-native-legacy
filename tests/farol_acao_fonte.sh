#!/bin/bash
# Acoes de fonte do Farol (issue #15, Onda 2, ADENDO 2.1 C): elegibilidade,
# escolha, filtro temporario so desta reproducao, "esconder DV nesta TV" com
# Desfazer e fontefalha.txt intocado. Lista sintetica; sem SDL em execucao,
# sem rede, sem player.
#
#   bash tests/farol_acao_fonte.sh
#   SANITIZE=1 bash tests/farol_acao_fonte.sh      # ASan + UBSan
set -eu
cd "$(dirname "$0")/.."

if grep -n "_Thread_local" src/farolacao_fonte.c src/farolacao_fonte.h; then
  echo "farol_acao_fonte: _Thread_local proibido" >&2; exit 1
fi
# O executor nao pode mexer na rota de canal ao vivo (fora do escopo da #15).
if grep -n -E "urlCanalRota|retentarCanal" src/farolacao_fonte.c src/farolacao_fonte.h; then
  echo "farol_acao_fonte: canal ao vivo fora do escopo" >&2; exit 1
fi

flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -Isrc -pthread)
if pkg-config --exists sdl2 2>/dev/null; then
  read -r -a sdk <<< "$(pkg-config --cflags sdl2)"
  flags+=("${sdk[@]}")
fi
ccbin="${CC:-cc}"
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread)
    flags+=(-fsanitize=thread)
    [ -z "${CC:-}" ] && command -v clang >/dev/null 2>&1 && ccbin=clang
    ;;
esac

work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-acao-fonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
mkdir "$work/data"

"$ccbin" "${flags[@]}" tests/farol_acao_fonte.c src/farolacao_fonte.c \
  src/fontefalha.c src/fontefiltro.c src/linguas.c -lm -o "$work/teste"
"$work/teste" "$work/data"
echo "farol_acao_fonte: tudo ok"
