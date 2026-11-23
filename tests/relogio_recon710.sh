#!/bin/bash
# #710: relogio da legenda na reabertura do TPK (412 -> 0 -> ... -> 412.3).
set -eu
cd "$(dirname "$0")/.."
cc -Wall -O1 tests/relogio_recon710.c src/relogio.c -o "${TMPDIR:-/tmp}/nuvio-relogio-recon710" -lm
"${TMPDIR:-/tmp}/nuvio-relogio-recon710"