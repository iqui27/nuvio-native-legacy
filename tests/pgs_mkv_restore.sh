#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-pgs-restore.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
cc -Isrc -O1 -g -Wall -Wextra tests/pgs_mkv_restore.c src/pgs_mkv_restore.c -lz -o "$DIR/t"
"$DIR/t"
