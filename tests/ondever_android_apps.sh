#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-ondever-android.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/ondever_android_apps.c src/js.c -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -std=gnu11 -pthread -Wall -Wextra -Wno-unused-function -o "$work/test"
"$work/test"
