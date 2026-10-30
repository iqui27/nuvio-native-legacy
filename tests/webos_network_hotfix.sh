#!/bin/bash
# Verify hotfix compatibility without creating a branch or changing a checkout.
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/queda-webos-build}"
mkdir -p "$TMPDIR"
tmp=$(mktemp -d "$TMPDIR/rede-hotfix.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
git archive v2.0.8 src/rede.c src/rede.h src/rede_pedido.inc src/redeurl.c \
  src/negcache.h src/negcache.inc src/addonurl.h | tar -x -C "$tmp"
git diff d887c23e4^ d887c23e4 -- src/rede.c > "$tmp/fix.patch"
(cd "$tmp" && git apply --check fix.patch && git apply fix.patch)
mkdir "$tmp/tests"
cp tests/rede_lifetime.c tests/rede_stress.c tests/rede_lifetime.sh tests/rede_pedido_server.py "$tmp/tests/"
(cd "$tmp" && bash tests/rede_lifetime.sh)
echo 'PASS: minimal network fix applies to v2.0.8 and regression passes'
