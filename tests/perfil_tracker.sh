#!/bin/bash
# Worker de Perfil/Stats sem rede, SDL inicializado ou dados pessoais.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-perfil-tracker-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc tests/perfil_tracker.c -Isrc -o "$tmp/teste" -O1 -g \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -Wl,-dead_strip -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"$tmp/teste"
