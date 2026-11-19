#!/bin/bash
# Execute production pause request and rating-selection code with offline doubles.
set -eu
cd "$(dirname "$0")/.."
python3 tests/pausao_ratings.py
