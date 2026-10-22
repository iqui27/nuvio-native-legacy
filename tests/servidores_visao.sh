#!/bin/bash
# Focused server-view regression; no renderer, network, credentials or account.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-servidores-visao.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/servidores_visao.c -Isrc -o "$work/test" -O1 -g \
  -ffunction-sections -fdata-sections -Wl,-dead_strip \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wno-deprecated-declarations -Wno-macro-redefined
"$work/test"
