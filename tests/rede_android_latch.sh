#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-android-latch.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -std=gnu11 -Wall -Wextra -Werror -Wno-unused-function -Isrc \
  tests/rede_android_latch.c src/redeurl.c -pthread -o "$tmp/test"
"$tmp/test"
