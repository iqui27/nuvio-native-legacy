#!/bin/bash
# StallGuard de HLS: fonte seca reabre antes da imagem congelar (src/livestall.h).
set -eu
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
cc -O1 -g -Wall -Wextra -Werror tests/livestall_seco.c -o "$T/nuvio-livestall-seco-tests"
"$T/nuvio-livestall-seco-tests"
