#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-c9-queda.XXXXXX")
trap 'rm -rf "$dir"' EXIT
cc -Wall -Wextra -Werror -Isrc tests/c9_queda.c -o "$dir/test"
"$dir/test" "$dir"
