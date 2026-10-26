#!/bin/bash
# P1 review regressions: production renderer, resource parser and keyboard actions.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-review.XXXXXX")
trap 'rm -rf "$work"' EXIT
NUVIO_REVIEW_P1=1 NUVIO_SHOT_BIN=/Volumes/ExternalSSD/tmp/rev/diretor21/21-j2/review-shot \
  bash tests/servidores_setup_shot.sh "$work"
