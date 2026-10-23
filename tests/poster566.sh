#!/bin/bash
# Offline regression for custom poster service templates (#566).
set -euo pipefail
cd "$(dirname "$0")/.."
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-poster566.XXXXXX")
trap 'rm -rf "$test_dir"' EXIT
cc -O1 -g -Wall -Wextra -Isrc src/posterprov.c src/redeurl.c tests/poster566.c \
  -lpthread -o "$test_dir/poster566"
"$test_dir/poster566"
