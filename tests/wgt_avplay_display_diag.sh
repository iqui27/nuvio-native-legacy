#!/bin/bash
# #703: tela preta com audio — contrato AVPlay setDisplayRect + thread principal.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'setDisplayRect' src/video_tizen.c
grep -q 'emscripten_is_main_browser_thread' src/video_tizen.c
grep -q 'setDisplayRect RECUSOU' src/video_tizen.c
echo "PASS: video_tizen com retangulo e diagnostico de recusa (#703)"
