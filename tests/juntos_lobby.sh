#!/bin/bash
# Capture and verify only the Watch Together lobby against its mock.
set -eu
cd "$(dirname "$0")/.."
review=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j5/r7-lobby
mkdir -p "$review/native"
bash tests/juntos_shot.sh "$review/native/n" --lobby
for frame in "$review"/native/n-*.bmp; do
  name=${frame##*/}
  mv "$frame" "$review/native/${name#n-}"
done
python3 tools/juntos_proofs.py --lobby
python3 tools/juntos_proofs.py --lobby --check
