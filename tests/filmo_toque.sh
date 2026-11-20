#!/bin/bash
set -eu
export TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp
bin=$(mktemp "$TMPDIR/filmo-touch.XXXXXX")
trap 'rm -f "$bin"' EXIT
cc -DNV_TOUCH_UI tests/filmo_toque.c -Isrc -I/opt/homebrew/include -Wl,-dead_strip -Wl,-undefined,dynamic_lookup -o "$bin"
"$bin"
