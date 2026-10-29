#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Production URL/thread lifecycle for the Hero curation (#205, review): saved
# URL must load during Home startup; clearing the URL must clear and fence off
# a pending fetch. Real curadoria.c; only disk, HTTP and SDL ticks are stubbed.
#   bash tests/curadoria_lifecycle.sh
set -eu
cd "$(dirname "$0")/.."
flags=()
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc ${flags[@]+"${flags[@]}"} src/js.c tests/curadoria_lifecycle.c \
  -Isrc -I/opt/homebrew/include -pthread -o "${TMPDIR:-/tmp}/nuvio-curadoria-lifecycle" -O1 -g \
  -Wall -Werror -Wno-deprecated-declarations -Wno-macro-redefined
"${TMPDIR:-/tmp}/nuvio-curadoria-lifecycle" startup
"${TMPDIR:-/tmp}/nuvio-curadoria-lifecycle" clear
"${TMPDIR:-/tmp}/nuvio-curadoria-lifecycle" pending
"${TMPDIR:-/tmp}/nuvio-curadoria-lifecycle" change
