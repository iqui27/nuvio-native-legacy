#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# #387: segurar OK nao dispara no quadro que sai de uma pausa longa.
set -euo pipefail
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-segurar.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc src/segurar.c tests/segurar.c -Isrc -I/opt/homebrew/include -O1 -g -Wall -Wextra \
  -o "$DIR/test"
"$DIR/test"
