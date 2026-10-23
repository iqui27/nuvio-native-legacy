#!/bin/bash
# Production card/hero placement and Tizen hole: committed FAIL, current PASS.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nv-card-rect.XXXXXX")
trap 'rm -rf "$work"' EXIT
rc=0
python3 tests/home-trailer-rect.py 98ed41d91 > "$work/baseline.log" 2>&1 || rc=$?
if [ "$rc" != 1 ] || ! grep -q '^FAIL expanded card video plane' "$work/baseline.log" ||
   ! grep -q '^FAIL expanded card visible video' "$work/baseline.log"; then
  cat "$work/baseline.log"
  exit 1
fi
echo 'PASS committed baseline fails card placement and visible geometry (rc=1)'
python3 tests/home-trailer-rect.py
