#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
${CC:-cc} -std=c11 -D_DEFAULT_SOURCE -pthread tests/dados-sync-rearm.c -o "$tmp/dados-sync-rearm"
"$tmp/dados-sync-rearm"
