#!/bin/sh
set -e
cd "$(dirname "$0")/.."
t=${TMPDIR:-/tmp}/dts_tv.$$
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/dts_tv.c -o "$t"
"$t"; rm -f "$t"
