#!/bin/bash
# Lembrete de jogo casado com o canal (src/esportes_casar.c) sobre um guia de
# mentira, com o lembrete e o ajuste gravados em disco de verdade.
set -eu
cd "$(dirname "$0")/.."
bash tests/esportes_canal.sh
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-esportes-casar.XXXXXX")
trap 'rm -rf "$work"' EXIT
export NUVIO_DADOS="$work/dados"
mkdir -p "$NUVIO_DADOS"
read -r -a sdl <<< "$(pkg-config --cflags sdl2 2>/dev/null || echo -I/opt/homebrew/include)"
cc -std=gnu11 -Isrc "${sdl[@]}" -O1 -g -Wall -Wextra -Werror -Wno-format-truncation \
  src/esportes_casar.c src/esportes_canal.c src/esportes_parse.c src/lembrete.c src/dados.c src/js.c src/buscanorm.c \
  tests/esportes_casar.c -pthread -lm -o "$work/test"
"$work/test"
