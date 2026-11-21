#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/audio_regional.c \
  src/audioescolha.c src/audioinfo.c src/linguas.c -o "${TMPDIR:-/tmp}/nuvio-audio-regional"
"${TMPDIR:-/tmp}/nuvio-audio-regional"
echo "audio_regional.sh: ok"
