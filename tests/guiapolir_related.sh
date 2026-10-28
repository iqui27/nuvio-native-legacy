#!/bin/bash
# Existing guide regressions plus the local production build, run once after UI fixes.
set -euo pipefail
cd "$(dirname "$0")/.."
bash tests/guiav5.sh
bash tests/guiav5_bugs.sh
bash tests/guianav.sh
bash tests/guia_previa_grande.sh
bash tests/guiav5_epg_lifetime.sh
bash tests/ajustes_secoes.sh
python3 tests/guiapolir_i18n.py
bash tools/build-local.sh
