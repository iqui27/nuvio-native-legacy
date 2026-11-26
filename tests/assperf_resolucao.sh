#!/bin/bash
# Render resolution modes (src/assperf.c, owner 19/11/2026).
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-assperf-res.XXXXXX")
trap 'rm -rf "$DIR"' EXIT
${CC:-cc} -DNV_ASSPERF_TESTE -Isrc -O1 -g -Wall -Wextra tests/assperf_resolucao.c src/assperf.c -pthread -o "$DIR/t"
"$DIR/t"
