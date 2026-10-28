#!/bin/bash
# Real parser, text rasterizer and Sources sheet, with offline fixtures only.
set -eu
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp}"
export NV_ADDON_TEST_FLAGS=-DNV_ADDON_LAYOUT_TEST
bash tests/fonte_addon_texto_shot.sh "${TMPDIR%/}/rev/fonteaddon/after"
