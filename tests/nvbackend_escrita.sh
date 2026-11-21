#!/bin/bash
# TOCTOU da persistencia do backend URL: worker da descoberta pausado ANTES
# de dados_gravar (--wrap) enquanto o usuario limpa, troca, ou troca e
# descobre de novo. O disco — e o arranque — tem de ficar com a escolha do
# usuario. Reproduz o probe do revisor sol-f31r2 (rc 6 no codigo falho).
#
#   bash tests/nvbackend_escrita.sh        (SANITIZE=1 para ASan/UBSan, SANITIZE=thread para TSan)
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nvescrita.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir/dados"
mkdir -p "$NUVIO_DADOS"
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread); fi
cc ${flags[@]+"${flags[@]}"} -O1 -g -Wall -Wextra -Isrc \
  tests/nvbackend_escrita.c src/nvbackend.c src/dados.c src/js.c \
  -o "$task_dir/teste" -DUNIT_TEST -lpthread -Wl,--wrap=dados_gravar
for caso in limpa troca reinicia; do
  "$task_dir/teste" "$caso"
done
