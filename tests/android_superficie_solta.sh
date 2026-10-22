#!/bin/bash
# Recriacao da superficie de video sem prender o fio principal (ver o .py).
set -euo pipefail
cd "$(dirname "$0")/.."
python3 tests/android_superficie_solta.py
