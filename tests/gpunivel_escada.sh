#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc src/gpunivel.c src/perfiltv.c tests/gpunivel_escada.c -Isrc -o "${TMPDIR:-/tmp}/nuvio-gpunivel-escada" \
  -DNV_GPUN_TESTE -DNV_WEBOS -O1 -g -Wall -Wextra -framework OpenGL -Wno-deprecated-declarations \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2
rc=0
for scenario in manual-effects frost-automatic; do
  if "${TMPDIR:-/tmp}/nuvio-gpunivel-escada" "$scenario"; then
    echo "$scenario: PASS"
  else
    echo "$scenario: FAIL"
    rc=1
  fi
done
"${TMPDIR:-/tmp}/nuvio-gpunivel-escada" || rc=1
exit "$rc"
