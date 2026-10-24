#!/bin/bash
# Small math regression: no app build, renderer, network or account.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-tela21.XXXXXX")
trap 'rm -rf "$work"' EXIT
if [ "$(uname -s)" = Darwin ]; then
  strip=(-Wl,-dead_strip)
else
  strip=(-Wl,--gc-sections)
fi
# Probe the actual migrated origins, not a duplicate formula.
for screen in biblioteca comecar conquistas filmo; do
  case "$screen" in
    biblioteca) origin='return paginaX();';;
    comecar) origin='origem(); return ox + MARGEM;';;
    conquistas) origin='origem(); return ox + MG;';;
    filmo) origin='return paginaX() + 72;';;
  esac
  printf '#include "%s/src/%s.c"\nfloat t21_probe_%s(void) { %s }\n' \
    "$PWD" "$screen" "$screen" "$origin" > "$work/$screen.c"
done
cc tests/tela21.c "$work"/*.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -ffunction-sections -fdata-sections "${strip[@]}" -lm -o "$work/test"
"$work/test"
