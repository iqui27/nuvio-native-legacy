#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/herocat_regras.c -o ${TMPDIR:-/tmp}/nuvio-herocat-regras && ${TMPDIR:-/tmp}/nuvio-herocat-regras
