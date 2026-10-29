#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
tmp=$(mktemp -d "$TMPDIR/rewatch-alvo.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Wall -Wextra -Isrc src/rewatch.c src/proximo.c tests/rewatch_alvo.c -lpthread -o "$tmp/test"
"$tmp/test"
