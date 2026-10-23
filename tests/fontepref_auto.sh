#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# A fonte do AUTOMATICO tambem e lembrada pelo bingeGroup (2.0.4, #310).
#   bash tests/fontepref_auto.sh
# 1. contrato de app.c (o que o host nao executa);
# 2. comportamento: tests/fontepref_auto.c.
set -euo pipefail
cd "$(dirname "$0")/.."

falha=0
confere() { if [ "$1" -ge "$2" ]; then echo "ok    $3"; else echo "FALHA $3"; falha=1; fi; }

# So grava depois de a fonte ter TOCADO (video pronto e andando), nunca ao escolher.
confere "$(grep -c 'fontepref_auto_guardar' src/app.c)" 1 "app.c grava a automatica"
n=$(grep -n 'fontepref_auto_guardar' src/app.c | head -1 | cut -d: -f1)
confere "$(sed -n "$((n - 25)),${n}p" src/app.c | grep -c 'video_pronto()')" 1 "so com video_pronto"
confere "$(sed -n "$((n - 25)),${n}p" src/app.c | grep -c 'video_pos()')" 1 "e com o video andando"
confere "$(sed -n "$((n - 25)),${n}p" src/app.c | grep -c 'fonteVODAutomatica')" 1 "so quando foi o automatico"
# "Escolher a fonte ao reproduzir": a automatica nao pode suprimir a folha.
confere "$(grep -c 'ajustes_fonte_manual() ? fontepref_escolher_manual' src/app.c)" 1 "modo manual usa a leitura so-manual"
# A folha nao marca a automatica como "Sua escolha anterior".
confere "$(grep -c 'stream_preferir(base\[0\] ? fontepref_escolher(base) : -1)' src/app.c)" 0 "folha nao usa a leitura com automatica"
[ "$falha" = 0 ] || exit 1

NUVIO_DADOS=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-fontepref-auto.XXXXXX)
export NUVIO_DADOS
trap 'rm -rf "$NUVIO_DADOS"' EXIT

sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/fontepref.c) continue;; esac
  sources+=("$source")
done

cc "${sources[@]}" tests/fontepref_auto.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-fontepref-auto \
  -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wall -Wextra -Wno-deprecated-declarations -Wno-macro-redefined
${TMPDIR:-/tmp}/nuvio-fontepref-auto "$@"
