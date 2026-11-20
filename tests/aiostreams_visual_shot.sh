#!/bin/bash
# Synthetic 1080p states in both materials; no network or navigation input.
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-/Volumes/ExternalSSD/tmp/rev/j-21-aio-visual-followup/after}
AIO_SHOT_STATES=1 AIO_SHOT_STATES_ONLY=1 bash tests/aiostreams_shot.sh "$out"
for state in full-config manifest-only error locked; do
  for material in solid glass; do
    test "$(magick identify -format '%wx%h' "$out/aio-$state-$material.png")" = 1920x1080
  done
done
printf 'AIOStreams: eight offline 1920x1080 state captures verified\n'
