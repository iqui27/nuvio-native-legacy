#!/bin/bash
# Estatisticas ao vivo do player: a conta e a origem por plataforma
# (src/estatvivo.c), sem SDL. Ver tests/estatvivo.c.
#   bash tests/estatvivo.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-estatvivo.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -Isrc tests/estatvivo.c src/estatvivo.c -o "$work/t" -O1 -g -Wall -Wextra
"$work/t"
