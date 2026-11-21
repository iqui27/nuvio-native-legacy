#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-farol-sonda.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/farol_sonda_server.py "$tmp/port" &
srv=$!
for i in $(seq 50); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port"
base="http://127.0.0.1:$(cat "$tmp/port")"
flags=(-Wall -Wextra -Werror -O1 -g -std=gnu11 -Isrc -pthread)
[ "$(uname -s)" = Darwin ] || flags+=(-ldl)
case "${SANITIZE:-0}" in
  1) flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);;
  thread) flags+=(-fsanitize=thread -fno-omit-frame-pointer);;
esac
"${CC:-cc}" "${flags[@]}" "-DFAROL_SONDA_WORKER=\"$base\"" tests/farol_sonda.c src/farolsonda.c src/farolanel.c src/rede.c src/redeurl.c -o "$tmp/teste"
"$tmp/teste" "$base" >"$tmp/log" 2>&1
python3 - "$tmp/log" "$base" <<'PY'
import sys
text = open(sys.argv[1]).read()
assert sys.argv[2] not in text
assert 'synthetic' not in text
PY
"${CC:-cc}" "${flags[@]}" "-DFAROL_SONDA_WORKER=\"$base/slow\"" tests/farol_sonda.c src/farolsonda.c src/farolanel.c src/rede.c src/redeurl.c -o "$tmp/lento"
"$tmp/lento" "$base/slow" slow-worker
"${CC:-cc}" "${flags[@]}" -D__EMSCRIPTEN__ tests/farol_sonda_wasm.c src/farolsonda.c -o "$tmp/wasm"
"$tmp/wasm"
echo 'farol_sonda: tudo ok'
