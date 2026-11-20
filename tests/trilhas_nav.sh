#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/trilhas_nav.py
bash tests/trilhas.sh
bash tests/conquistas_regressao.sh
bash tests/inicio_trilhas.sh
