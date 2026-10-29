#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-seekgesto"
cc tests/seekgesto.c -o "$out" -O1 -g -Wall -lm && "$out"
