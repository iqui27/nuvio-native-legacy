#!/bin/bash
# The audio track picked by hand comes back on every episode, after a restart
# too (src/audiomemoria.c). Pure code with a fake disk, no SDL.
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -Isrc tests/audiomemoria.c src/audiomemoria.c src/audioescolha.c \
  src/audioinfo.c src/linguas.c -o "${TMPDIR:-/tmp}/nuvio-audiomemoria"
"${TMPDIR:-/tmp}/nuvio-audiomemoria"
