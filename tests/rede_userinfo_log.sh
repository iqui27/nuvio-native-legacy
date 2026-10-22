#!/bin/bash
# Userinfo da URL nao pode aparecer em log algum (hostDaUrl, caminhos Android).
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rede-userinfo.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
flags=(-std=gnu11 -O0 -g -Wall -I src -DNV_ANDROID=1 -DNV_DESC_MIN_MS=0ull)
cc "${flags[@]}" tests/rede_userinfo_log.c src/redeurl.c -o "$tmp/t" -lpthread -ldl
"$tmp/t" "$tmp/out.txt"
