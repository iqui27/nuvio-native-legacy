#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Reconexao quando a rede cai no meio do video (src/video_reconexao.h).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/reconexao.c -o ${TMPDIR:-/tmp}/nuvio-reconexao-tests
${TMPDIR:-/tmp}/nuvio-reconexao-tests
