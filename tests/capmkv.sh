#!/bin/bash
# 203-capitulos: capitulos do MKV no Android/.tpk (ver tests/capmkv.c).
#   bash tests/capmkv.sh
set -euo pipefail
cd "$(dirname "$0")/.."
#   SANITIZE=1 (padrao) ASAN+UBSAN, SANITIZE=thread TSAN, SANITIZE=0 nenhum.
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-capmkv.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -Wno-unused-function -Wno-deprecated-declarations tests/capmkv.c src/capmkv.c src/mkv.c -lpthread -o "$DIR/test"
"$DIR/test"
