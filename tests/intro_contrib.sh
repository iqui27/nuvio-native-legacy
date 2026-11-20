#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
cc tests/intro_contrib.c src/segfontes.c src/js.c src/credfonte.c src/capmkv.c src/mkv.c \
  -Isrc -o "${TMPDIR%/}/nuvio-intro-contrib" -O1 -g \
  -Wall -Wno-deprecated-declarations -Wno-macro-redefined -lpthread
"${TMPDIR%/}/nuvio-intro-contrib"
