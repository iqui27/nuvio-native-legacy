#!/bin/bash
# The donor's subtitle match (src/doadorleg.c) against the subtitle sync
# engine (src/autosync.c), on synthetic index timelines. No FFmpeg, no network.
# With a log file: replays its "[doadorleg] points" pairs (the real tracks).
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
o=$(mktemp -d "$TMPDIR/nuvio-doadorleg.XXXXXX")
trap 'rm -rf "$o"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Wno-misleading-indentation -Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc \
  tests/doadorleg.c src/doadorleg.c src/autosync.c src/legenda.c src/assperf.c src/assfios.c src/assrender.c src/dts/doador_tpk.c \
  -pthread -lm -o "$o/t"
"$o/t" "$@"
