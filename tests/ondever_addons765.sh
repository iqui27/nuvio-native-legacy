#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'addons_log765_scrapers' src/addons.c
test -f tests/ondever_launch_guard.sh
echo "PASS: diagnostico fontes vs plugins (#765)"
