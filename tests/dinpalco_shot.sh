#!/bin/bash
# Real 1920x1080 rendering and routing checks, using local artwork only.
set -eu
cd "$(dirname "$0")/.."
export NV_DINPALCO=matrix NV_DINPALCO_AUDIT=1
export NV_TIPOS='trend_series=13,drama_movie=17'
export NV_AJ='socialLocal 1
palcoFundoLocal 2'
bash tests/homelayouts_shot.sh "${1:-/Volumes/ExternalSSD/tmp/rev/dinpalco/after}" 20
