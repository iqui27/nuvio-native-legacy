#!/usr/bin/env bash
# Bounded redacted range diagnostics (Matis94 C3). Spins the synthetic server,
# drives the real chunk/stream range callers and checks the [range-diag] lines
# and the redacted QR export.
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-rdiag.XXXXXX")
server=''
trap 'if [ -n "$server" ]; then kill "$server" 2>/dev/null || true; fi; rm -rf "$task_tmp"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -keyout "$task_tmp/key.pem" -out "$task_tmp/cert.pem" \
  -subj /CN=127.0.0.1 -addext subjectAltName=IP:127.0.0.1 >/dev/null 2>&1
python3 tests/dts_range_server.py "$task_tmp/ports" "$task_tmp/cert.pem" "$task_tmp/key.pem" &
server=$!
for _ in $(seq 50); do [ -s "$task_tmp/ports" ] && break; sleep .1; done
read -r http_port peer_port tls_port < "$task_tmp/ports"
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -DNV_HTTP_BUFFER_DIAG -Isrc)
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/range_diagnostics.c src/rede.c src/redeurl.c src/logexport.c \
  -ldl -lpthread -o "$task_tmp/test"
"$task_tmp/test" "$http_port" > "$task_tmp/raw.log"
"$task_tmp/test" --export "$task_tmp/raw.log" > "$task_tmp/export.log"
python3 - "$task_tmp" <<'PY'
from pathlib import Path
import sys
root = Path(sys.argv[1])
raw = (root / 'raw.log').read_text()
exported = (root / 'export.log').read_text()
lines = [line for line in raw.splitlines() if line.startswith('[range-diag] ')]
events = [dict(field.split('=', 1) for field in line.split()[1:]) for line in lines]
assert events and all(line in exported for line in lines), 'Diagnostics survive QR redaction'
assert all(secret not in raw and secret not in exported for secret in (
    'hidden-query-secret', 'hidden-header-secret', 'hidden-body-secret',
    'hidden-request-secret', 'hidden-version-secret'))
assert any(e['http'] == '429' and e['retrySec'] == '7' and e['server'] == 'nginx' for e in events)
assert any(e['http'] == '429' and 110 <= int(e['retrySec']) <= 120 for e in events)
assert any(e['retrySeen'] == '1' and e['retrySec'] == '0' for e in events)
assert any(e['http'] == '302' and e['retrySec'] == '99' for e in events)
assert any(e['http'] == '429' and e['hop'] == '1' and e['retrySeen'] == '0' and e['retrySec'] == '-1'
           for e in events), 'Redirect headers must not leak into the final response'
assert any(e['kind'] == 'stream' and e['http'] == '429' for e in events)
assert any(e['kind'] == 'stream' and e['http'] == '200' and e['bytes'] == '32' for e in events)
assert 1 <= sum(e['http'] == '206' for e in events) <= 8, 'Healthy chunks must be sampled'
assert max(int(e['activeAtStart']) for e in events) >= 2
assert all(e['host'] == 'address' for e in events), 'Numeric addresses remain private'
print('PASS: HTTP 429, Retry-After delta/date/reset, stream/chunk metrics, sampling, concurrency and QR privacy')
PY
