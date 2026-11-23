#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cc -std=c11 -Wall -Wextra -Werror -pthread -Isrc tests/rotahost.c src/rotahost.c -o "$work/test"
"$work/test"
