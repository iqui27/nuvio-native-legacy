#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d "${TMPDIR:-/tmp}/cloud-flow.XXXXXX"); trap 'rm -rf "$t"' EXIT
cc -std=gnu11 -Isrc -I/opt/homebrew/include -DNV_REC_URL='"https://fixture.invalid"' -DNV_LOGLINK_TOKEN='"synthetic-upload-only"' \
  tests/logcloud_flow.c src/logcloud.c src/logexport.c -o "$t/test" -lpthread -lz
"$t/test" > "$t/output"
if rg -q '0123456789abcdef|synthetic-upload-only|PRIVATE_FIXTURE|fixture.invalid' "$t/output"; then
  echo 'FAIL: sensitive value in upload output'; exit 1
fi
cat "$t/output"
