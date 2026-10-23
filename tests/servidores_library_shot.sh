#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
output=/Volumes/ExternalSSD/tmp/rev/servidores-library-png
mkdir -p "$output"
NV_MODO=servers bash tests/biblioteca_shot.sh "$output/native"
for input in "$output"/native-*.bmp; do
  name=${input##*/native-}; name=${name%.bmp}
  magick "$input" "$output/$name-app.png"
done
python3 tools/servidores_png_check.py "$output"
