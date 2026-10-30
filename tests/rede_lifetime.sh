#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export TMPDIR="${TMPDIR:-/Volumes/ExternalSSD/tmp/queda-webos-build}"
mkdir -p "$TMPDIR"
tmp=$(mktemp -d "$TMPDIR/rede-lifetime.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread)
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl); fi
cc "${flags[@]}" tests/rede_lifetime.c src/redeurl.c -o "$tmp/test"
"$tmp/test"
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost \
  -keyout "$tmp/key.pem" -out "$tmp/cert.pem" > "$tmp/certificate.log" 2>&1
python3 tests/rede_pedido_server.py "$tmp/ports" "$tmp/cert.pem" "$tmp/key.pem" \
  > "$tmp/server.log" 2>&1 &
srv=$!
for i in $(seq 50); do [ -s "$tmp/ports" ] && break; sleep 0.1; done
test -s "$tmp/ports"
cc "${flags[@]}" tests/rede_stress.c src/redeurl.c -o "$tmp/stress"
# Network logs can contain request addresses. Keep them private; print only verdict.
if "$tmp/stress" "$(head -1 "$tmp/ports")" "$(tail -1 "$tmp/ports")" \
    > "$tmp/stress.log" 2>&1; then
  tail -1 "$tmp/stress.log"
else
  echo 'FAIL: local network stress (request logs withheld)' >&2
  python3 - "$tmp/stress.log" <<'PY'
import pathlib, re, sys
text = pathlib.Path(sys.argv[1]).read_text()
print('curl results:', re.findall(r'curl=(\d+)', text), file=sys.stderr)
for line in text.splitlines():
    if re.match(r'^Assertion failed: \([A-Za-z0-9_ .&=!<>+%\[\]()-]+\), function \w+, file [\w.]+, line \d+\.$', line):
        print(line, file=sys.stderr)
PY
  exit 1
fi
