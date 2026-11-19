#!/bin/bash
# Cache negativo de rede (src/negcache.c). Sem rede.
#   bash tests/negcache.sh
set -eu
cd "$(dirname "$0")/.."
bin="${TMPDIR:-/tmp}/nuvio-negcache-tests"
cc -O1 -g -Wall -pthread tests/negcache.c -o "$bin"
"$bin"
