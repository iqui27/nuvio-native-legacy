#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/ajustes-pin-conta.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc tests/ajustes_pin_conta.c src/js.c src/jsw.c -Isrc -O1 -ffunction-sections -fdata-sections -Wl,-dead_strip -Wno-deprecated-pragma -o "$work/check"
"$work/check"
