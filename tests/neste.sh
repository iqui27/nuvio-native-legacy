#!/bin/bash
# Neste episodio: parse de guest_stars do TMDB e estado inicial.
set -eu
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/nuvio-neste-test"
cc -Wall -Wextra -O2 tests/neste.c src/neste.c src/js.c -Isrc -o "$out" -lpthread
"$out"
rm -f "$out"
