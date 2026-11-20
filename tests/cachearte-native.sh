#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -std=gnu11 -Wall -Wextra -Werror -Isrc tests/cachearte.c src/cachearte.c \
  -pthread -o ${TMPDIR:-/tmp}/nuvio-cachearte-native
${TMPDIR:-/tmp}/nuvio-cachearte-native
