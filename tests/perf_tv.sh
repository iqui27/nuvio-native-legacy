#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
if [[ ! -f src/perftv.h ]]; then
  echo 'FAIL: perf-tv formatter/aggregator missing' >&2
  exit 1
fi
cc -std=c11 -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include \
  src/fpscena.c src/perftv.c tests/perf_tv.c -lm -o "$tmp/perf-tv"
"$tmp/perf-tv"
python3 tests/perf_tv_tools.py
