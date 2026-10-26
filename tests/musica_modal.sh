#!/bin/bash
# Music modal on Detail: OK on the soundtrack card opens an animated modal (no
# page push), Back closes it back onto the card. Needs a GL window.
set -eu
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/musicamodal
mkdir -p "$out"
NV_MUSICA_ONLY=1 bash tests/detail_secoes_shot.sh "$out/detail"
