#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

# Do not touch a host's pre-existing Tizen developer directory if one is present.
if [ -w /media/developer/temp/nuvio ]; then
  echo "SKIP: /media/developer/temp/nuvio is writable on this host"
  exit 0
fi

${CC:-cc} -std=c11 -D_DEFAULT_SOURCE -pthread tests/dados-persistencia.c -o "$tmp/dados-persistencia"
"$tmp/dados-persistencia"
