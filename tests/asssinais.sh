#!/bin/bash
# Signs & Songs matching (src/asssinais.c, owner 19/11/2026): hash, libass's
# sign rule, track names, partner blocks from the relay, tolerance, generation.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-asssinais.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
${CC:-cc} -Isrc -O1 -g -Wall -Wextra ${SANITIZE:+-fsanitize=address,undefined} tests/asssinais.c src/asssinais.c -pthread -o "$DIR/t"
"$DIR/t"
