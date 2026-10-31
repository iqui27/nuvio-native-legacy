#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d)
trap 'rm -rf "$task_tmp"' EXIT
flags=(-O1 -g -Isrc -Wall -Wextra -Wno-unused-parameter -DCEL_VALIDADE_S=3)
if [[ ${CEL_TEST_LOOPBACK:-0} == 1 ]]; then flags+=(-DNV_CEL_TEST_LOOPBACK); fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" src/celular.c src/logexport.c tests/celular_export.c -o "$task_tmp/test" -lpthread
"$task_tmp/test"
