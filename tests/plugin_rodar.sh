#!/bin/bash
# Runs one scraper; without arguments, checks the runner with a local JS fixture.
set -euo pipefail
cd "$(dirname "$0")/.."
[ -f tests/plugin_rodar.c ] || { echo 'missing tests/plugin_rodar.c' >&2; exit 1; }
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-plugin-rodar.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -pthread \
  -Wno-macro-redefined -ffunction-sections -fdata-sections -Wl,-dead_strip \
  src/qjs.c src/pluginjs.c src/plugrede.c src/htmlq.c src/rede.c src/redeurl.c \
  src/stream_parse.c src/fontefiltro.c src/linguas.c src/badges.c src/js.c tests/plugin_rodar.c -o "$tmp/rodar"
if [ "$#" -gt 0 ]; then "$tmp/rodar" "$@"; exit; fi
printf '%s\n' 'module.exports.getStreams = async () => [{name:"Runner",url:"https://example.org/test.m3u8",quality:"1080p"}];' > "$tmp/test.js"
"$tmp/rodar" "$tmp/test.js" runner 27205 movie > "$tmp/log"
grep -q 'fontes=1 (mapeadas 1)' "$tmp/log"
printf '%s\n' 'module.exports.getStreams = () => { throw new Error("runner failure"); };' > "$tmp/test.js"
if "$tmp/rodar" "$tmp/test.js" runner 27205 movie; then exit 1; fi
echo 'plugin_rodar: ok'
