#!/bin/bash
# Watch Together: relogio NTP, decisao de correcao A/B, protecoes e mensagens.
#   bash tests/juntossync.sh
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-juntossync.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -std=gnu11 -O1 -g -Wall -Wextra -Werror -Isrc src/juntossync.c src/juntosproto.c src/js.c tests/juntossync.c -lm -o "$work/t"
"$work/t"
