#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
dir=$(mktemp -d "$TMPDIR/rw-account.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -O1 -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip tests/rewatch_account.c src/tirarremoto.c -o "$dir/test"
"$dir/test"
