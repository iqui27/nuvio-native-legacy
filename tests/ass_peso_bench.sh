#!/bin/bash
# Spike de medida: custo por quadro de ASS (libass) e PGS (src/pgs.c), por
# cena e por resolucao de render. Imprime uma tabela; nao aprova nem reprova.
#   bash tests/ass_peso_bench.sh
# Os resultados e a leitura estao em docs/plans/ass-pgs-peso.md.
set -eu
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "ass_peso_bench: libass ausente (medida ignorada)"
  exit 0
fi
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ass-peso.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc -O2 -g -Wall -Wextra "${flags[@]}" tests/ass_peso_bench.c src/pgs.c -o "$DIR/bench" $(pkg-config --cflags --libs libass)
"$DIR/bench" "$@"
