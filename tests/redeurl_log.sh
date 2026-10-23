#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# rede_url_log: config/chave de provedor de poster/meta nunca vai para o log.
set -eu
cd "$(dirname "$0")/.."
DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-redeurl-log.XXXXXX)
trap 'rm -rf "$DIR"' EXIT
cc -Wall -Wextra -Werror -O1 -g tests/redeurl_log.c src/redeurl.c -o "$DIR/t"
"$DIR/t"
