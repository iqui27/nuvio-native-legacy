#!/bin/bash
# Seletor do .so da release para .tpk/.tpk40 — ver tests/atualizacao_tpk.c.
#   bash tests/atualizacao_tpk.sh [--sanitize]
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-atualizacao-tpk.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
flags=(-O1 -g -Isrc -pthread -ffunction-sections -fdata-sections -Wno-deprecated-declarations)
# ld64 nao tem --gc-sections; -dead_strip e o equivalente (mesma receita de
# tests/addonslista.sh).
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
if [ "${1:-}" = --sanitize ] || [ "${SANITIZE:-0}" = 1 ]; then
  flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer)
fi
for platform in NV_TPK NV_TPK40; do
  platform_flags=(-DNV_TPK)
  [ "$platform" = NV_TPK40 ] && platform_flags+=(-DNV_TPK40)
  cc "${flags[@]}" "${platform_flags[@]}" -include src/tpk.h tests/atualizacao_tpk.c src/redeurl.c src/js.c -o "$DIR/t-$platform" -ldl -lm
  "$DIR/t-$platform"
done
