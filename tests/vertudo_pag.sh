#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc tests/vertudo_pag.c -Isrc -o ${TMPDIR:-/tmp}/nuvio-vertudo-pag -O1 -g
${TMPDIR:-/tmp}/nuvio-vertudo-pag
