#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-js-limites.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -O1 -g -Wall -Wextra -fsanitize=address,undefined -Isrc \
  src/js.c tests/js_limites.c -o "$DIR/teste"
"$DIR/teste"
