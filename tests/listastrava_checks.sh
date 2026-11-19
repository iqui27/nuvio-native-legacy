#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/listastrava_auth.py
python3 tests/listastrava_names.py
python3 tests/listastrava_assets.py
python3 tools/android-art-check.py build/android/assets/art
git diff --check
