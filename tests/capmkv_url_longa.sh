#!/bin/bash
# Capitulos do MKV com URL assinada comprida (ver tests/capmkv_url_longa.c).
#   bash tests/capmkv_url_longa.sh
set -euo pipefail
cd "$(dirname "$0")/.."
#   SANITIZE=1 (padrao) ASAN+UBSAN, SANITIZE=thread TSAN, SANITIZE=0 nenhum.
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-capmkv-longa.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc ${flags[@]+"${flags[@]}"} -Isrc -Itests -O1 -g -Wall -Wextra -Wno-unused-function -Wno-deprecated-declarations tests/capmkv_url_longa.c src/capmkv.c src/mkv.c -lpthread -o "$DIR/test"
"$DIR/test"
