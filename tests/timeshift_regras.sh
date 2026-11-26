#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/timeshift_regras.c -o ${TMPDIR:-/tmp}/nuvio-timeshift-regras && ${TMPDIR:-/tmp}/nuvio-timeshift-regras
