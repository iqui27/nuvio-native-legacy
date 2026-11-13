#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/audioext-signal.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT
python3 tests/dts_external_fixtures.py "$tmp"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Wno-unused-function tests/dts_external_signal.c -lm -o "$tmp/test"
"$tmp/test" "$tmp"
