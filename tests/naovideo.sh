#!/bin/bash
# Fonte que nao e video (2.0.2). Ver src/naovideo.h.
#
#   bash tests/naovideo.sh
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Isrc src/naovideo.c tests/naovideo.c -o "${TMPDIR:-/tmp}/nuvio-naovideo-tests"
"${TMPDIR:-/tmp}/nuvio-naovideo-tests"
