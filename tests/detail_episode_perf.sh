#!/bin/bash
# Hidden-window GL regression: real Modern detail, glass, ten episode cards.
set -eu
cd "$(dirname "$0")/.."
export NV_EP_PERF=1
export NV_AJ=$'vidroLocal 0\nhomeLayoutLocal 0\nfundoLocal 0\nselected_theme 1\nescalaTextoLocal 2\nescalaTextoVersaoLocal 1'
bash tests/detail_eps_shot.sh "${1:-/tmp/nv205-episode}"
