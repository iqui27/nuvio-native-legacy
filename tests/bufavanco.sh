#!/bin/bash
# Veto do watchdog de buffer por avanco do fim do buffer (src/bufavanco.h).
set -eu
cd "$(dirname "$0")/.."
T="${TMPDIR:-/tmp}"
cc -O1 -g -Wall -Wextra -Werror -Isrc tests/bufavanco.c -o "$T/nuvio-bufavanco-tests"
"$T/nuvio-bufavanco-tests"
