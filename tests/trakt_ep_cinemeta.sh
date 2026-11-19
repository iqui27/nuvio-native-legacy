#!/bin/bash
# #356: episodio "a seguir" ausente na ficha do Nuvio mas presente no Cinemeta.
#   bash tests/trakt_ep_cinemeta.sh
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-trakt-ep.XXXXXX")
trap 'rm -rf "$dir"' EXIT
ld=(-Wl,-dead_strip)
ff=(-ffunction-sections -fdata-sections)
if [ "$(uname -s)" != Darwin ]; then
  ld=(-Wl,--gc-sections)
fi
inc=(-Isrc)
[ -d /opt/homebrew/include ] && inc+=(-I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
cc src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/trakt_ep_cinemeta.c \
  "${inc[@]}" "${ff[@]}" "${ld[@]}" \
  -o "$dir/teste" -O1 -g -Wall -Wextra -lpthread
"$dir/teste" | tail -3
