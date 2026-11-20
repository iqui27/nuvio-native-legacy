#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/xtream-transport.XXXXXX")
server_pid=
trap '[[ -z "$server_pid" ]] || kill "$server_pid" 2>/dev/null; rm -rf "$work"' EXIT
extra=()
transport=src/rede.c
if [[ ${XTREAM_BASELINE:-0} == 1 ]]; then
  git show HEAD:src/rede.c > "$work/rede.c"
  extra=(-DTRANSPORT_UNDER_TEST=\"$work/rede.c\")
  transport="$work/rede.c"
fi
cc -std=gnu11 -O1 -Isrc ${extra[@]+"${extra[@]}"} tests/xtream_transport.c src/redeurl.c -pthread -o "$work/test"
failed=0
if ! ("$work/test" > "$work/log" 2>&1) 2>/dev/null; then failed=1; fi
cc -std=gnu11 -O1 -Isrc ${extra[@]+"${extra[@]}"} tests/xtream_slow.c "$transport" src/redeurl.c -pthread -o "$work/slow"
python3 tests/xtream_slow_server.py "$work/port" > "$work/server-log" 2>&1 &
server_pid=$!
for ((i=0;i<100;i++)); do [[ ! -s "$work/port" ]] || break; sleep 0.05; done
port=$(<"$work/port")
if ! ("$work/slow" "$port" > "$work/slow-log" 2>&1) 2>/dev/null; then failed=1; fi
if [[ $failed == 0 ]]; then echo 'Xtream transport: PASS'; else echo 'Xtream transport: FAIL (request output withheld)'; fi
exit "$failed"
