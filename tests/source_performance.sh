#!/usr/bin/env bash
# Focused CPU/lifecycle regressions for the upstream-only performance port.
# SANITIZE=1 enables ASan/UBSan; renderer coverage: tests/fontes_formatter.sh.
set -euo pipefail
cd "$(dirname "$0")/.."
for test in source_scroll source_selection source_audio_cache source_badge_readiness source_clipping source_backdrop; do
  python3 "tests/$test.py"
done
bash tests/source_loading_vsync.sh
bash tests/fontenota.sh
bash tests/fonteauto.sh
