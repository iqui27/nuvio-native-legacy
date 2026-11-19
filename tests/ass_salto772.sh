#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q '#772: salto ASS' src/player.c
echo "PASS: guarda de salto ASS no relogio (#772)"
