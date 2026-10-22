#!/bin/bash
# Seek recusado (webOS 700): tentativas com espera, sem derrubar a fonte.
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/seekretry.c -o "${TMPDIR:-/tmp}/nuvio-seekretry-tests"
"${TMPDIR:-/tmp}/nuvio-seekretry-tests"
