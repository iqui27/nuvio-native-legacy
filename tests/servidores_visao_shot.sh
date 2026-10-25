#!/bin/bash
# The server overview is the Library with origins, not the server browser.
set -euo pipefail
cd "$(dirname "$0")/.."
output=${1:-/Volumes/ExternalSSD/tmp/rev/servidores21-png}
mkdir -p "$output"
NV_MODO=servers bash tests/biblioteca_shot.sh "$output/native"
for input in "$output"/native-overview-*.bmp; do
  name=${input##*/native-}
  magick "$input" "$output/${name%.bmp}.png"
done
