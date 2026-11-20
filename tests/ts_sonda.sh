#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/ts_sonda.c -o ${TMPDIR:-/tmp}/nuvio-ts-sonda && ${TMPDIR:-/tmp}/nuvio-ts-sonda
