#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -euo pipefail
cd "$(dirname "$0")/.."
binary=$(mktemp ${TMPDIR:-/tmp}/nuvio-player-seekr-status.XXXXXX)
trap 'rm -f "$binary"' EXIT
case "$(uname -s)" in
  Darwin) stripflag=-Wl,-dead_strip ;;
  *) stripflag=-Wl,--gc-sections ;;
esac
cc tests/player_seekr_status.c src/seekr.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -g -ffunction-sections -fdata-sections "$stripflag" \
  -Wno-deprecated-declarations -Wno-macro-redefined -o "$binary"
"$binary"
