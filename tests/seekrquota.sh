#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
set -eu
cd "$(dirname "$0")/.."
quota_dir=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-seekrquota.XXXXXX)
trap 'rm -rf "$quota_dir"' EXIT
cc src/dados.c tests/seekrquota.c -Isrc -pthread -O1 -g -Wall -Wextra \
  -o ${TMPDIR:-/tmp}/nuvio-seekrquota-tests
NUVIO_DADOS="$quota_dir" ${TMPDIR:-/tmp}/nuvio-seekrquota-tests
