#!/bin/bash
# Review C-R3: RAM floor holds with "Reduzir o buffer com pouca memória" ON and OFF.
#   bash tests/buffer_ram_reduz.sh
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-bufreduz.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc src/cacheboost.c tests/buffer_ram_reduz.c -lpthread -lm -o "$work/t"
"$work/t" | grep -v '^\[buffer\]'
