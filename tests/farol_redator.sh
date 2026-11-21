#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bin=$(mktemp "${TMPDIR:-.}/nuvio-farol-redator.XXXXXX")
trap 'rm -f "$bin"' EXIT
${CC:-cc} -Wall -Wextra -O1 -g ${CFLAGS:-} tests/farol_redator.c src/farolredator.c src/farolerro.c src/fontesonda.c -Isrc -o "$bin"
"$bin"
echo "farol_redator: tudo ok"
