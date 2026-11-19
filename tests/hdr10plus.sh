#!/bin/bash
# HDR10+ dynamic SEI strip (issue #605): pure unit test of the Annex-B filter.
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror tests/hdr10plus.c -fsanitize=address,undefined \
  -o "${TMPDIR:-/tmp}/nuvio-hdr10plus"
"${TMPDIR:-/tmp}/nuvio-hdr10plus"
