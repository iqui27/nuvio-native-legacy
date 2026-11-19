#!/bin/bash
# #369 item 2: libass desenha o tailandes com o NotoSansThai embarcado.
#   bash tests/tailandes_ass.sh
set -euo pipefail
cd "$(dirname "$0")/.."
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists libass; then
  echo "tailandes_ass: libass ausente (teste ignorado)"; exit 0
fi
out="${TMPDIR:-/tmp}"
cc tests/tailandes_ass.c src/assrender.c -o "$out/nuvio-tailandes-ass" -Isrc \
  $(pkg-config --cflags --libs libass) -Wall -Wno-deprecated-declarations 2>&1 | grep -v "^$" || true
"$out/nuvio-tailandes-ass" deploy/app/fonts
