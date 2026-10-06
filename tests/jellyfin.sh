#!/bin/bash
# F11 Jellyfin against a local fake server (tests/jellyfin_server.py).
#   bash tests/jellyfin.sh
#   NV_SANITIZERS=1 bash tests/jellyfin.sh     # ASan + UBSan
#   NV_TSAN=1 bash tests/jellyfin.sh           # ThreadSanitizer
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-jellyfin.XXXXXX")
srv=""
trap '[ -z "$srv" ] || kill "$srv" 2>/dev/null || true; rm -rf "$tmp"' EXIT
python3 tests/jellyfin_server.py "$tmp/port" &
srv=$!
for _ in $(seq 50); do [ -s "$tmp/port" ] && break; sleep 0.1; done
test -s "$tmp/port"
base=$(sed -n '1p' "$tmp/port")
mkdir -p "$tmp/dados"
flags=(-O1 -g -std=gnu11 -Wall -Wextra -Werror -Isrc -pthread)
# streams.h includes <SDL2/SDL.h> (types only; SDL is not linked).
flags+=(-I/opt/homebrew/include)
if command -v sdl2-config >/dev/null; then
  # shellcheck disable=SC2207
  flags+=($(sdl2-config --cflags) -I"$(sdl2-config --prefix)/include")
fi
if [ "$(uname -s)" != Darwin ]; then flags+=(-ldl); fi
if [ "${NV_SANITIZERS:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
if [ "${NV_TSAN:-0}" = 1 ]; then flags+=(-fsanitize=thread -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/jellyfin.c src/jellyfin.c src/rede.c src/redeurl.c src/js.c src/jsw.c \
  -o "$tmp/teste"
"$tmp/teste" "$base" "$tmp/dados" > "$tmp/log" 2>&1 || { cat "$tmp/log"; exit 1; }
# tests/servidores_log.sh collects every server test's log and scans it.
if [ -n "${NV_SRV_LOG:-}" ]; then cat "$tmp/log" >> "$NV_SRV_LOG"; fi
# Logs carry operation/status/latency only: no token, password or media path.
if grep -E 'pwtoken|qctoken|api_key|ApiKey|Token=|quoted|/srv/private|QcSecret' "$tmp/log"; then
  echo 'secret leaked into the log' >&2; exit 1
fi
grep -c '^\[jellyfin\]' "$tmp/log" | sed 's/^/jellyfin log lines: /'
tail -n 5 "$tmp/log" | grep -v '^\[jellyfin\]' || true
