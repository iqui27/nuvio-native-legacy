#!/bin/bash
# The desktop Sources CTA opens the add-on page (MENU_FONTES), so it must not
# reuse "Gerenciar fontes", whose sibling translations mean typefaces.
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'manage->action=200;.*i18n("Gerenciar add-ons")' src/streams_desktop.inc
for f in src/idioma_tab.h src/idioma_??.h src/idioma_????.h; do
  grep -q '"Gerenciar add-ons",' "$f" || { echo "FAIL: $f lacks Gerenciar add-ons"; exit 1; }
done
export TMPDIR=/Volumes/Cache/jobtmp
bin=$(mktemp "$TMPDIR/nuvio-cta.XXXXXXXX");trap 'rm -f "$bin"' EXIT
# Lookup and byte order of every table, the path i18n() uses.
cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include -o "$bin" -Wno-macro-redefined -Wno-deprecated-declarations
"$bin" >/dev/null
echo 'PASS: desktop Sources CTA uses the localized add-on key'
