#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
cc -O1 -g -Wall -Wextra -Werror ${SANITIZE:+-fsanitize=address,undefined} -Isrc \
  src/marcacena.c tests/marcacena.c -o "${TMPDIR:-/tmp}/nv-marcacena"
"${TMPDIR:-/tmp}/nv-marcacena"
