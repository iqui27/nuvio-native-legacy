#!/bin/bash
# PR 600: tamanhos EBML e profundidade hostis na sonda do MKV.
# Ver tests/mkv_hostil.c.
#   bash tests/mkv_hostil.sh
#   SANITIZE=0 desliga ASan+UBSan; NV_RUST_MKVCORE=1 roda contra o nucleo Rust.
set -euo pipefail
cd "$(dirname "$0")/.."
flags=()
case "${SANITIZE:-1}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer) ;;
  thread) flags+=(-fsanitize=thread) ;;
esac
. tests/mkvcore_flag.sh
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-mkvhostil.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc ${flags[@]+"${flags[@]}"} -Isrc -O1 -g -Wall -Wextra -Wno-unused-function -Wno-deprecated-declarations \
  tests/mkv_hostil.c src/mkv.c ${MKVCORE_CC[@]+"${MKVCORE_CC[@]}"} -lpthread -o "$DIR/test"
"$DIR/test"
