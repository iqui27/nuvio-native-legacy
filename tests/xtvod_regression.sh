#!/bin/bash
# Focused owner regression plus existing guide/Xtream/Sports/network checks.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/xtvod-checks.XXXXXX")
trap 'rm -rf "$work"' EXIT
for check in xtream_transport xtream guiav5 guiav5_bugs esportes esportes_estado esportes_ui rede_android_latch; do
  if bash "tests/$check.sh" > "$work/$check.log" 2>&1; then
    echo "$check: PASS"
  else
    echo "$check: FAIL (request output withheld)"
    exit 1
  fi
done
