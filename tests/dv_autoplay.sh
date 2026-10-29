#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
bash tests/fontefalha_playback.sh
python3 tests/android_recria_hdr.py
python3 tests/android_window_layout.py
bash tests/android_superficie_solta.sh
