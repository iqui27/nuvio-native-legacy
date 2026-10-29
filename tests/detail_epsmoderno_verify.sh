#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 tests/detail_epmeta.py
python3 tests/detail_epinfo.py
bash tests/detail_eps.sh
bash tests/detail_episode_review.sh
bash tests/detail_epsmoderno.sh
bash tests/detail_epslista.sh
bash tests/detail21.sh
