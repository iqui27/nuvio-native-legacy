#!/bin/bash
# Vigia de trava do canal ao vivo (src/livestall.h).
set -eu
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
cc -O1 -g -Wall -Wextra -Werror tests/livestall.c -o "$T/nuvio-livestall-tests"
"$T/nuvio-livestall-tests"
