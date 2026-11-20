#!/bin/bash
# src/redeurl.c entra aqui e src/rede.c NAO: tests/debrid.c finge o transporte
# (rede_postar_st / rede_baixar_st), entao linkar rede.c inteiro daria simbolo
# duplicado. redeurl.c e so a redacao de credencial do log, sem rede nenhuma —
# e o teste exercita a de verdade em vez de um stub que nao esconde nada.
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-debrid.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DEBRID_TEST_LOG="$task_dir/saida.txt"
cc ${flags[@]+"${flags[@]}"} -Dtime=debrid_test_time -pthread -O1 -g -Wall -Wextra -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wno-deprecated-declarations \
  src/debrid.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/js.c src/redeurl.c tests/debrid.c -o "$task_dir/teste"
"$task_dir/teste"
