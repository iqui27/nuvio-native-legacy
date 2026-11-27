#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-badge-regex.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
cc -O1 -g -Wall -Wextra -Werror -Isrc src/regexjs.c tests/regexjs_badge.c -pthread -o "$test_dir/test"
"$test_dir/test"
if [[ ${SANITIZE:-0} == 1 ]]; then
  cc -O1 -g -Wall -Wextra -Werror -Isrc src/regexjs.c tests/regexjs_badge.c -pthread \
    -fsanitize=address,undefined -fno-omit-frame-pointer -o "$test_dir/sanitized"
  "$test_dir/sanitized"
fi
