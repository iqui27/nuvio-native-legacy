#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d /tmp/nuvio-exclusion-languages.XXXXXX)
trap 'rm -rf "$dir"' EXIT
flags=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then flags+=(-Wl,-dead_strip); else flags+=(-Wl,--gc-sections); fi
cc -O1 -g -Isrc -I/opt/homebrew/include "${flags[@]}" \
  src/stream_parse.c src/js.c src/badges.c src/fontefiltro.c src/linguas.c \
  tests/source_exclusion_languages.c -o "$dir/test"
rc=0
for finding in accents mixed; do
  if "$dir/test" "$finding"; then echo "$finding: PASS"; else rc=1; fi
done
exit "$rc"
