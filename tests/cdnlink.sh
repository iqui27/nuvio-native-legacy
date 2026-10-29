#!/bin/bash
# cdnlink.c sem rede (ver o topo de tests/cdnlink.c).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc tests/cdnlink.c src/cdnlink.c -lpthread -o "${TMPDIR:-/tmp}/nuvio-cdnlink"
"${TMPDIR:-/tmp}/nuvio-cdnlink"
