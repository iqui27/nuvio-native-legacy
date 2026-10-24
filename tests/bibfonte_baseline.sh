#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
rc=0
output=$(BIBFONTE_REV=1d3ac4e60^ bash tests/bibfonte.sh 2>&1) || rc=$?
printf '%s\n' "$output"
[ "$rc" -eq 1 ]
rg -q '^FAIL: detail/context save getters follow fallback$' <<<"$output"
rg -q '^FAIL: Trakt-only title excluded under Simkl$' <<<"$output"
rg -q '^bibfonte: [1-9][0-9]* failure\(s\)$' <<<"$output"
printf 'PASS: baseline compiles and reaches source/content assertions\n'
