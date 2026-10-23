#!/bin/bash
# "." under (?s) must match a leading newline. The first-byte filter used to
# skip \n/\r even with dotAll.
#
#   bash tests/regexjs_dotall.sh
set -eu
cd "$(dirname "$0")/.."
D=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-regexjs-dotall.XXXXXX")
trap 'rm -rf "$D"' EXIT
cc -O2 -g -Wall -Wextra -Isrc src/regexjs.c tests/regexjs_dotall.c -o "$D/dotall"
"$D/dotall"
