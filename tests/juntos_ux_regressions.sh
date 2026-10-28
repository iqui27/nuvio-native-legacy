#!/bin/bash
# Existing Watch Together engine, Android ordering, auth and source contracts.
set -euo pipefail
cd "$(dirname "$0")/.."
for check in juntos_fluxo juntos_tcl juntos_62 juntos_auth juntos_400 juntossync juntos_sim juntosfonte juntos_e2e travajuntos; do
  bash "tests/$check.sh"
done
python3 tests/juntos_android_pause.py
printf 'juntos_ux_regressions: OK\n'
