#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-linguas.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/linguas.c src/linguas.c -Isrc -std=gnu11 -g -Wall -Wextra -Wno-unused-function -o "$work/t"
"$work/t"
