#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc src/perfiltv.c tests/trailerimdb_auto.c -o "${TMPDIR:-/tmp}/nuvio-trailerimdb-auto"
"${TMPDIR:-/tmp}/nuvio-trailerimdb-auto"
