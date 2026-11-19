#!/bin/bash
# #739: desambiguacao do indice 3 do posterProvLocal pelo marcador prov=.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-d1-739.XXXXXX")
trap 'rm -rf "$work"' EXIT
cc -Wall -Wextra -Isrc tests/d1_poster_migrar.c -o "$work/test"
"$work/test"
printf 'd1_poster_migrar.sh: ok\n'
