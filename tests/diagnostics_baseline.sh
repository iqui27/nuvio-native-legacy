#!/bin/bash
# Reproduce runtime red evidence without modifying a worktree or stash refs.
set -euo pipefail
cd "$(dirname "$0")/.."
t=$(mktemp -d "${TMPDIR:-/tmp}/diagnostics-baseline.XXXXXX"); trap 'rm -rf "$t"' EXIT
for base in 76aa42819 13d121214; do
  mkdir "$t/$base"
  git archive "$base" servidor src | tar -x -C "$t/$base"
  cp servidor/recomendacoes/teste-loglink.mjs "$t/$base/servidor/recomendacoes/"
  if (cd "$t/$base" && node --test --test-timeout=5000 servidor/recomendacoes/teste-loglink.mjs) > "$t/$base.log" 2>&1; then
    echo "FAIL: expected baseline $base to lack the new secure link behavior"; exit 1
  fi
  if [[ $base == 76aa42819 ]]; then
    grep -q '^not ok 1 - post' "$t/$base.log"
    grep -q '^not ok 2 - expiracao' "$t/$base.log"
    if bash tests/log_snapshot_http.sh "$t/$base" > "$t/snapshot.log" 2>&1; then
      echo 'FAIL: pre-port server unexpectedly served snapshot'; exit 1
    fi
    grep -q 'FAIL: HTTP endpoint did not serve the frozen log snapshot' "$t/snapshot.log"
  else
    grep -q '^not ok 3 - escopo' "$t/$base.log"
    grep -q '200 !== 401' "$t/$base.log"
  fi
  echo "PASS: baseline $base fails the expected runtime HTTP/security assertions"
done
