#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-aio-statistics.XXXXXX")
server=""
trap '[ -z "$server" ] || kill "$server" 2>/dev/null || true; rm -rf "$work"' EXIT
cc -O1 -g -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections -Wl,-dead_strip tests/aio_statistics.c src/aiostreams.c src/addonstats.c src/js.c src/stream_parse.c src/fontefiltro.c src/linguas.c src/badges.c -DNV_ADDONSTATS_PURO -o "$work/test"
"$work/test"
openssl req -x509 -newkey rsa:2048 -nodes -days 1 -subj /CN=localhost -keyout "$work/key.pem" -out "$work/cert.pem" > "$work/certificate.log" 2>&1
python3 tests/aio_network_server.py "$work/ports" "$work/cert.pem" "$work/key.pem" &
server=$!
for i in $(seq 50); do [ -s "$work/ports" ] && break; sleep 0.1; done
{ read -r plain; read -r tls; } < "$work/ports"
cc -O1 -g -pthread -Isrc -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -ffunction-sections -fdata-sections -Wl,-dead_strip tests/aio_network.c src/rede.c src/redeurl.c src/addonstats.c src/aiostreams.c src/js.c -DNV_ADDONSTATS_PURO -o "$work/network"
"$work/network" "$plain" "$tls"
