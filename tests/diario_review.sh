#!/bin/bash
# Focused regressions for diary account isolation, persistence and playback gates.
set -eu
cd "$(dirname "$0")/.."
bash tests/diario.sh
bash tests/diarionuvem.sh
bash tests/trava.sh
bash tests/travajuntos.sh
