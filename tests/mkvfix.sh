#!/bin/bash
# mkvfix.c without network (see the top of tests/mkvfix.c).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -DNV_MKVFIX_SEM_REDE -Isrc tests/mkvfix.c src/mkvfix.c -o "${TMPDIR:-/tmp}/nuvio-mkvfix"
"${TMPDIR:-/tmp}/nuvio-mkvfix"
