#!/bin/bash
# Sonda do MKV que falha: nova tentativa com espera, fora do buffering (DV na C9).
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror tests/dvretry.c -o "${TMPDIR:-/tmp}/nuvio-dvretry-tests"
"${TMPDIR:-/tmp}/nuvio-dvretry-tests"
