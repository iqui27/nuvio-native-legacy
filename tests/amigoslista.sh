#!/bin/bash
# Ordem dos amigos e corte das sugestoes (src/amigoslista.h). Sem GL.
set -euo pipefail
cd "$(dirname "$0")/.."
tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-amigoslista-XXXXXX")"
trap 'rm -rf "$tmp"' EXIT
cc -Wall tests/amigoslista.c -o "$tmp/t" && "$tmp/t"
