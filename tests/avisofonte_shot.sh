#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
export NUVIO_SHOT_AVISOFONTE=1
export NUVIO_SHOT_IDIOMA=0
bash tests/detalhe_glass_shot.sh /Volumes/ExternalSSD/tmp/rev/avisofonte movie-alert
