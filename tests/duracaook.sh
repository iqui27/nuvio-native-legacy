#!/bin/bash
# "Manter esta fonte" no aviso de duracao (ver tests/duracaook.c).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Isrc tests/duracaook.c src/duracaook.c -lpthread -o "${TMPDIR:-/tmp}/nuvio-duracaook"
"${TMPDIR:-/tmp}/nuvio-duracaook"
