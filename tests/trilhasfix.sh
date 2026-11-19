#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/trilhasfix.py
bash tests/trilhas_nav.sh
bash tests/caca.sh
bash tests/caca_ownership.sh
