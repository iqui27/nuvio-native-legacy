#!/bin/bash
# Debounce das remontagens da Home (src/descdebounce.h).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/descdebounce.c -o "${TMPDIR:-/tmp}/nuvio-descdebounce-tests"
"${TMPDIR:-/tmp}/nuvio-descdebounce-tests"
