#!/bin/bash
# The automatic audio order (src/audioescolha.c). Pure code, no SDL.
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/audioescolha.c src/audioescolha.c src/audioinfo.c src/linguas.c \
  -o "${TMPDIR:-/tmp}/nuvio-audioescolha"
"${TMPDIR:-/tmp}/nuvio-audioescolha"
