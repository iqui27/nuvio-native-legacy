#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ws-url.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/watchstate_url.c src/watchstate.c src/js.c src/jsw.c -Isrc -std=gnu11 -Wall -Wextra -lpthread -o "$work/t"
"$work/t"
