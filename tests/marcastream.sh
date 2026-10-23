#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-marcastream.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Wall -Wextra -Isrc tests/marcastream.c -o "$tmp/table"
"$tmp/table"
cc -O1 -g -Wall -Wextra -Isrc tests/marcastream_worker.c src/js.c -pthread -o "$tmp/worker"
rc=0
"$tmp/worker" config || rc=1
"$tmp/worker" network || rc=1
"$tmp/worker" key || rc=1
exit "$rc"
