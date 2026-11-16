#!/bin/bash
# TorBox burn-in (tbburn.h): playlist/JSON parsing, the createstream route,
# plan refusals, eligibility and the playback row. Stubs, no network.
#   bash tests/tbburn.sh     SANITIZE=1 (ASan/UBSan)
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-tbburn.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all); fi
cc "${flags[@]}" -Isrc -O1 -g -Wall -Wextra src/tbburn.c tests/tbburn.c -pthread -o "$D/t"
"$D/t"
cc -Isrc -DNV_TPK -fsyntax-only -Wall -Wextra -Werror src/tbburn.c
echo "tbburn ok"
