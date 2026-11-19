#!/bin/bash
# "Ja assisti" / resposta a uma recomendacao (recresp.c, atividade.c, reacao.c).
#
#   bash tests/recresp.sh
#
# NV_REC_URL de mentira (o servico "existe"), sem fio de rede ligado. NUVIO_DADOS
# aponta para uma pasta temporaria: o teste grava recomendacoes-respostas.txt.
set -eu
cd "$(dirname "$0")/.."
tmp="${TMPDIR:-/tmp}"
sources=()
for source in src/*.c src/dts/*.c; do
  [ "$source" = src/main.c ] && continue
  sources+=("$source")
done
bin="$tmp/nuvio-recresp-tests"
cc "${sources[@]}" tests/recresp.c -Isrc -o "$bin" \
  -O1 -g -DATIV_SEM_FIO '-DNV_REC_URL="http://127.0.0.1:9"' \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
NUVIO_DADOS=$(mktemp -d "$tmp/nuvio-recresp-dados.XXXXXX")
export NUVIO_DADOS
r=0
"$bin" || r=$?
rm -rf "$NUVIO_DADOS" "$bin" "$bin.dSYM"
exit $r
