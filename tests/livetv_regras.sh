#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/livetv_regras.c -o ${TMPDIR:-/tmp}/nuvio-livetv-regras && ${TMPDIR:-/tmp}/nuvio-livetv-regras
