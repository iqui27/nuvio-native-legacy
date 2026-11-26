#!/bin/bash
# Anel em disco do timeshift. NUVIO_TS_DIR=/caminho/do/pendrive mede outro volume.
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
cc -Wall -Wextra tests/timeshift_disco.c -lpthread -o ${TMPDIR:-/tmp}/nuvio-timeshift-disco
${TMPDIR:-/tmp}/nuvio-timeshift-disco "${NUVIO_TS_DIR:-${TMPDIR:-/tmp}/nuvio-timeshift-disco.$$}"
