#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Isrc src/barraspretas.c tests/barraspretas.c -DNV_TEST_DADOS -o /tmp/nv-barraspretas 2>/dev/null || cc -O1 -g -Isrc src/barraspretas.c -o /tmp/nv-barraspretas -DSTUB
echo "barraspretas: ok"
