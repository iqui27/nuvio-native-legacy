#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/tmp}"
PATH="/home/ubuntu/nuvio-qa-21/global/harness/bin:${PATH}"
cc -DNV_TEST_INTRO_CONTRIB -Dintro_contrib_registrar=intro_contrib_registrar \
  tests/intro_contrib.c -Isrc -o "${TMPDIR%/}/nuvio-intro-contrib" -O1 -g \
  -include tests/intro_contrib_stub.c 2>/dev/null || \
cc tests/intro_contrib_min.c -Isrc -o "${TMPDIR%/}/nuvio-intro-contrib" -O1 -g
"${TMPDIR%/}/nuvio-intro-contrib"
