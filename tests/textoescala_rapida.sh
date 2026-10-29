#!/bin/bash
# Quick subset of tests/textoescala_prova.sh (under a minute): View all (posters
# in grid/ranking/saga/line layouts plus the side panel art) and the Player
# pause panel, at 100% vs 120%, this tree only. NV_SRC=<old tree> must FAIL.
cd "$(dirname "$0")/.."
NV_ONLY_NEW=1 NV_SHOTS="${NV_SHOTS:-vertudo player_pausa}" exec bash tests/textoescala_prova.sh
