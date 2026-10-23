#!/bin/bash
export TMPDIR="${TMPDIR:-/tmp}"
TMPDIR="${TMPDIR%/}"
# Local HTTP/TLS fixtures; no SDL and no external network.
set -eu
cd "$(dirname "$0")/.."
DTS_RANGE_DIR=$(mktemp -d ${TMPDIR:-/tmp}/nuvio-dts-range.XXXXXX)
DTS_RANGE_SERVER_PID=''
trap 'if [ -n "$DTS_RANGE_SERVER_PID" ]; then kill "$DTS_RANGE_SERVER_PID" 2>/dev/null || true; fi; rm -rf "$DTS_RANGE_DIR"' EXIT
openssl req -x509 -newkey rsa:2048 -nodes -days 1 \
  -keyout "$DTS_RANGE_DIR/key.pem" -out "$DTS_RANGE_DIR/cert.pem" \
  -subj /CN=127.0.0.1 -addext subjectAltName=IP:127.0.0.1 \
  >/dev/null 2>&1
python3 tests/dts_range_server.py "$DTS_RANGE_DIR/ports" "$DTS_RANGE_DIR/cert.pem" "$DTS_RANGE_DIR/key.pem" &
DTS_RANGE_SERVER_PID=$!
for _ in $(seq 50); do [ -s "$DTS_RANGE_DIR/ports" ] && break; sleep 0.1; done
[ -s "$DTS_RANGE_DIR/ports" ]
read -r DTS_RANGE_HTTP_PORT DTS_RANGE_PEER_PORT DTS_RANGE_TLS_PORT < "$DTS_RANGE_DIR/ports"
${CC:-gcc} -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc tests/dts_range.c src/rede.c src/redeurl.c \
  -ldl -lpthread -o "$DTS_RANGE_DIR/test"
"$DTS_RANGE_DIR/test" "$DTS_RANGE_HTTP_PORT" "$DTS_RANGE_PEER_PORT" "$DTS_RANGE_TLS_PORT" "$DTS_RANGE_DIR/cert.pem"
