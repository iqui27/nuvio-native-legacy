#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/regex-scoped.XXXXXX")
trap 'rm -rf "$D"' EXIT
if [ "${1:-}" = --baseline ]; then
  git show 76aa42819:src/regexjs.c > "$D/regexjs.c"
  git show 76aa42819:src/regexjs.h > "$D/regexjs.h"
  cc -O2 -I"$D" "$D/regexjs.c" tests/regexjs_scoped.c -o "$D/test"
else
  cc -O2 -Isrc src/regexjs.c tests/regexjs_scoped.c -o "$D/test"
fi
"$D/test"
