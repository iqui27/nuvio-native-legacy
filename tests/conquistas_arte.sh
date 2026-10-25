#!/bin/bash
# Conquistas 3D: familia/chave -> arquivo de arte e acento; os 104 PNG do pacote existem. Ver tests/conquistas_arte.c.
set -eu
cd "$(dirname "$0")/.."
t=$(mktemp "${TMPDIR:-/tmp}/nuvio-cqarte.XXXXXX")
trap 'rm -f "$t"' EXIT
cc -Wall -Wextra -O1 -g tests/conquistas_arte.c src/conquistasarte.c -Isrc -o "$t"
"$t"
for o in director actor decade genre noir fantasy horror saga studio top250 country language secret community trail list explorer award international palm season silent movement discovery; do
  for m in bronze silver gold locked; do [ -f "deploy/app/art/conquistas/$o-$m.png" ] || { echo "falta $o-$m.png"; exit 1; }; done
done
