#!/bin/bash
# Plex: parsers on fixtures, PIN state machine, id/GUID matching and the whole
# integration against a local fake plex.tv + PMS (tests/plex_server.py).
#   bash tests/plex.sh
#   NV_SANITIZERS=1 bash tests/plex.sh     # ASan + UBSan
#   NV_TSAN=1 bash tests/plex.sh           # ThreadSanitizer
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-plex.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/plex_server.py "$tmp/port" &
srv=$!
for _ in $(seq 50); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port"
port=$(sed -n '1p' "$tmp/port")
base="http://127.0.0.1:$port"
mkdir -p "$tmp/dados"
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread)
flags+=(-I/opt/homebrew/include)
if command -v sdl2-config >/dev/null; then
  # shellcheck disable=SC2207
  flags+=($(sdl2-config --cflags) -I"$(sdl2-config --prefix)/include")
fi
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl); fi
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${NV_TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
# plex.tv is the fake too: the override is compile-time only.
flags+=("-DNV_PX_TV=\"$base\"")
cc "${flags[@]}" tests/plex.c src/plex.c src/jellyfin.c src/rede.c src/redeurl.c src/js.c src/jsw.c \
  -o "$tmp/teste"
"$tmp/teste" "$base" "$tmp/dados" > "$tmp/log" 2>&1 || { cat "$tmp/log"; exit 1; }
# tests/servidores_log.sh collects every server test's log and scans it.
if [ -n "${NV_SRV_LOG:-}" ]; then cat "$tmp/log" >> "$NV_SRV_LOG"; fi
# Logs carry operation/status/latency only: no token, private path, e-mail or address.
if grep -E 'plexacct|plexsrv|owned-srv|tok-fixture|/srv/private|private@example|X-Plex-Token|127\.0\.0\.1|WXYZ' "$tmp/log"; then
  echo 'secret leaked into the log' >&2; exit 1
fi
grep -c '^\[plex\]' "$tmp/log" | sed 's/^/plex log lines: /'
tail -n 4 "$tmp/log" | grep -v '^\[plex\]' || true
