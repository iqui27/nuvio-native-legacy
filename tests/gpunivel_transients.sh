#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-gpunivel-transients.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc src/gpunivel.c src/perfiltv.c tests/gpunivel_transients.c -Isrc -o "$work/test" \
  -DNV_GPUN_TESTE -DNV_WEBOS -O1 -g -Wall -Wextra -framework OpenGL -Wno-deprecated-declarations \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
"$work/test"
bash tests/gpunivel_escada.sh
