#!/bin/bash
# Subtitle delay sign regression (tests/legenda_atraso_sinal.c).
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-legsinal.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -Isrc -O1 -g -Wall -Wextra src/legenda2.c src/legenda.c src/assrender.c \
  tests/legenda_atraso_sinal.c -pthread -lm -o "$work/test"
"$work/test"
