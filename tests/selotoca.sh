#!/bin/bash
# Selo do que esta tocando: regra de resolucao e HDR medido (HLG, HDR10+).
set -eu
cd "$(dirname "$0")/.."
T=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-selotoca.XXXXXX"); trap 'rm -rf "$T"' EXIT
cc -Isrc -Wall -Wextra tests/selotoca.c -o "$T/t"
"$T/t"
