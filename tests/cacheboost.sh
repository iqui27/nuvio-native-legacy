#!/bin/bash
# F07 seek cache / volume boost state. SANITIZE=1 (ASan+UBSan) or
# SANITIZE=thread for the concurrent reporter.
#   bash tests/cacheboost.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-cacheboost.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${SANITIZE:-0}" = thread ]; then flags+=(-fsanitize=thread); fi
cc "${flags[@]}" src/cacheboost.c tests/cacheboost.c -lpthread -lm -o "$work/t"
"$work/t"
