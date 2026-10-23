#!/bin/bash
# Native (.tpk/Android/VIDAA) trailer selection is byte-for-byte the one of
# integracao/2.0.5: Trailer audio is a webOS/wasm feature only.
set -eu
cd "$(dirname "$0")/.."
T="$(mktemp -d "${TMPDIR:-/tmp}/nv-trailer-nativo.XXXXXX")"
trap 'rm -rf "$T"' EXIT
mkdir "$T/base"
git show integracao/2.0.5:src/trailerfonte.c > "$T/base/trailerfonte.c"
git show integracao/2.0.5:src/trailerfonte.h > "$T/base/trailerfonte.h"
for def in "" -DNV_TPK -DNV_VIDAA; do
  cc -O1 -Wall -I"$T/base" -Isrc -I/opt/homebrew/include $def tests/trailer-nativo-igual.c "$T/base/trailerfonte.c" -o "$T/velho"
  cc -O1 -Wall -Isrc -I/opt/homebrew/include $def tests/trailer-nativo-igual.c src/trailerfonte.c -o "$T/novo"
  "$T/velho" > "$T/velho.txt"; "$T/novo" > "$T/novo.txt"
  cmp -s "$T/velho.txt" "$T/novo.txt" || { echo "trailer-nativo-igual: FALHOU ${def:-default}"; diff "$T/velho.txt" "$T/novo.txt" | head; exit 1; }
done
echo "trailer-nativo-igual: tudo ok ($(wc -l < "$T/novo.txt") linhas x3)"
