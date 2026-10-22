#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-salto-suave"
cc tests/salto_suave.c -o "$out" -O1 -g -lm && "$out"
