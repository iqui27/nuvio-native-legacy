#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/audioext.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -pthread src/audioext.c tests/audioext.c -o "$tmp/audioext"
"$tmp/audioext"
