#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
task_dir="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-nvbackend.XXXXXX")"
trap 'rm -rf "$task_dir"' EXIT
export NUVIO_DADOS="$task_dir/dados"
mkdir -p "$NUVIO_DADOS"
cc -O1 -g -Wall -Wextra -Isrc tests/nvbackend.c src/nvbackend.c src/dados.c src/js.c \
  -o "$task_dir/teste" -DUNIT_TEST -lpthread
"$task_dir/teste"
