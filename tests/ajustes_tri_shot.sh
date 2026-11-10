#!/bin/bash
# Three-island Settings captures (2.1): every island state, glass and solid.
# Reuses the Settings capture harness; never touches accounts or the network.
#   bash tests/ajustes_tri_shot.sh [/Volumes/ExternalSSD/tmp/rev/ajustestri]
set -eu
cd "$(dirname "$0")/.."
output="${1:-/Volumes/ExternalSSD/tmp/rev/ajustestri}"
mkdir -p "$output"
export NUVIO_AJ_QUADROS="tri-indice tri-chips tri-secao tri-opcao tri-mais tri-editor tri-video-row tri-video-editor tri-numero tri-cor tri-presets tri-revisao tri-diferentes tri-parental tri-solido tri-lista"
export NUVIO_SHOT_BIN="$output/shot"
bash tests/ajustes_shot.sh "$output/app"
# A section change must animate the list (fade + slide, ~220 ms), and reduced
# motion must settle at once: the harness asserts both from the same binary.
NUVIO_AJ_QUADROS="tri-secao" NUVIO_SHOT_TECLAS= NUVIO_SHOT_MOTION=ld NUVIO_SHOT_MOTION_CHECK=1 "$NUVIO_SHOT_BIN" "$output/motion"
# The video-opening editor also has open, pending-value and Back strips.
mkdir -p "$output/motion"
NUVIO_AJ_QUADROS="tri-video-row" NUVIO_SHOT_MOTION=r NUVIO_SHOT_MOTION_FINO=1 "$NUVIO_SHOT_BIN" "$output/motion/video-open"
NUVIO_AJ_QUADROS="tri-video-editor" NUVIO_SHOT_MOTION=d NUVIO_SHOT_MOTION_FINO=1 "$NUVIO_SHOT_BIN" "$output/motion/video-value"
NUVIO_AJ_QUADROS="tri-video-editor" NUVIO_SHOT_MOTION=b NUVIO_SHOT_MOTION_FINO=1 "$NUVIO_SHOT_BIN" "$output/motion/video-back"
