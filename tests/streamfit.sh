#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
SF_DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-streamfit.XXXXXX)
trap 'rm -rf "$SF_DIR"' EXIT
flags=(-O1 -g -Wall -Wextra -Werror -Isrc -pthread)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
cc "${flags[@]}" tests/streamfit.c src/streamfit.c src/vazao.c -o "$SF_DIR/test"
"$SF_DIR/test"
