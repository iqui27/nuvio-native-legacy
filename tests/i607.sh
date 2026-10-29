#!/bin/bash
# Offline regression checks for the seven community review claims in #607.
set -euo pipefail
cd "$(dirname "$0")/.."
node tests/issue607.cjs
bash tests/issue607_subtitle.sh
# Exercise the real WGT scripts with local fixtures and a deliberately failing
# fake compiler. Never sign a package using an inherited certificate profile.
TIZEN_PERFIL= bash tests/tizen_wgt_stale.sh
