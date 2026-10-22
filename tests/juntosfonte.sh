#!/bin/bash
# Watch Together: impressao do release e veredito mesma/parecida/diferente/nenhuma.
#   bash tests/juntosfonte.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntosfonte.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include src/juntosfonte.c src/juntosproto.c \
  src/juntossync.c src/js.c tests/juntosfonte.c -lm -o "$work/t"
"$work/t"
