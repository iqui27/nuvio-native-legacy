#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-imdb-rating.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -Wall -Wextra -pthread ${NV_CFLAGS:-} -Isrc tests/imdbnota.c src/imdbnota.c -o "$work/test"
"$work/test"
