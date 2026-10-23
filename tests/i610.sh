#!/bin/bash
# Offline fixture proof; no servers, devices, or credentials.
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-i610.XXXXXX")
trap 'rm -rf "$dir"' EXIT
if [ "${1:-}" = --prove-before ]; then
  mkdir "$dir/before"
  git archive 688f08e53 src tests | tar -x -C "$dir/before"
  cp tests/i610.c "$dir/before/tests/i610.c"
  cc -DI610_TRAKT "$dir/before/tests/i610.c" "$dir/before/src/js.c" \
    -I"$dir/before/src" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -O1 -Wno-deprecated-declarations -ffunction-sections -fdata-sections \
    -Wl,-dead_strip -lpthread -o "$dir/before-test"
  if "$dir/before-test"; then echo "FAIL: baseline unexpectedly passed"; exit 1; fi
  echo "#610 baseline: expected FAIL"
fi
for provider in TRAKT SIMKL; do
  cc -DI610_${provider} tests/i610.c src/js.c src/traktscrobble.c src/metaep.c -Isrc \
    -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -O1 -g -Wno-deprecated-declarations -Wno-macro-redefined \
    -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread -o "$dir/$provider"
  "$dir/$provider"
done
cc tests/trakt610_auth.c src/credfio.c src/js.c src/jsw.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g \
  -Wno-deprecated-declarations -Wno-macro-redefined -lpthread -o "$dir/auth"
"$dir/auth"
cc -DI610_AUTH src/simklauth.c src/credfio.c src/js.c src/jsw.c tests/i610.c -Isrc \
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -O1 -g \
  -Wno-deprecated-declarations -Wno-macro-redefined -lpthread -o "$dir/simklauth"
"$dir/simklauth"
cc -DI610_ORDER tests/i610.c src/cwordem.c -Isrc -O1 \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread -o "$dir/order"
"$dir/order"
cc -DI610_RESUME src/trakt.c src/metaep.c tests/stub_fichameta.c src/js.c src/metaprov.c tests/i610.c \
  -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -O1 -Wno-deprecated-declarations -Wno-macro-redefined \
  -ffunction-sections -fdata-sections -Wl,-dead_strip -lpthread -o "$dir/resume"
"$dir/resume"
python3 - <<'PY'
import pathlib,re
for path in pathlib.Path("src").glob("idioma_*.h"):
    text=path.read_text()
    for key in ("Sair da conta", "Não foi possível agora. Tente novamente."):
        assert re.search(r'"'+re.escape(key)+r'",\s*"[^" ]',text), (path,key)
print("#610 sign-out and retry messages exist in every language: PASS")
PY
