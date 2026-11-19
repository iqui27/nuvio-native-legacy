#!/bin/bash
# Playback stream_fit decision: measured download vs declared bitrate (src/redefit.h).
set -eu
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
cc -O1 -g -Wall -Wextra -Werror -Isrc tests/redefit.c -o "$T/nuvio-redefit-tests"
"$T/nuvio-redefit-tests"
