#!/bin/bash
# #691: padrão rede_job_estado → rede_job_soltar (ASan).
set -euo pipefail
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nv-691.XXXXXX"); trap 'rm -rf "$D"' EXIT
flags=(-Isrc -O1 -g -Wall -Wextra -pthread)
if [ "${SANITIZE:-1}" = 1 ]; then
  flags+=(-fsanitize=address -fno-omit-frame-pointer)
fi
cc "${flags[@]}" tests/plugins_descarte691.c src/rede.c -o "$D/t"
"$D/t"
echo "PASS: plugins descarte #691 (rede job)"
