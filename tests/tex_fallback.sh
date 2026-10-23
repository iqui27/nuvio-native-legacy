#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
python3 tests/tex_fallback.py
