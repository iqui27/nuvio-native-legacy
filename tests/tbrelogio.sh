#!/bin/bash
# The subtitle clock during TorBox burn-in (tbrelogio.h), with log_18's numbers.
#   bash tests/tbrelogio.sh     SANITIZE=1 (ASan/UBSan)
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-tbrelogio.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc "${flags[@]}" -Isrc -O1 -g -Wall -Wextra -Werror src/tbrelogio.c tests/tbrelogio.c -pthread -lm -o "$D/t"
"$D/t"
