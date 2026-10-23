#!/bin/bash
# Personal servers: NO TOKEN IN ANY LOG. Runs the Jellyfin, Emby and Plex
# tests (fake servers, tests/*_server.py), keeps their whole stdout/stderr and
# fails if any line carries a credential or a private path:
#   - a query token, whatever the server calls it (ApiKey=, api_key=,
#     X-Plex-Token=, Token=, token=);
#   - an auth header value (MediaBrowser ... Token=, X-Emby-Token);
#   - any fixture token, password, Quick Connect secret or media path.
#   bash tests/servidores_log.sh
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-srvlog.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
export NV_SRV_LOG="$tmp/todos.log"
: > "$NV_SRV_LOG"
for t in jellyfin emby plex; do
  bash "tests/$t.sh" > "$tmp/$t.out" 2>&1 || { cat "$tmp/$t.out"; echo "tests/$t.sh failed" >&2; exit 1; }
done
linhas=$(wc -l < "$NV_SRV_LOG" | tr -d ' ')
[ "$linhas" -gt 100 ] || { echo "too few log lines collected ($linhas)" >&2; exit 1; }
padrao='ApiKey=|api_key=|X-Plex-Token|[Tt]oken=|X-Emby-Token|MediaBrowser |pwtoken|qctoken|QcSecret|embytoken|plexacct|plexsrv|tok-fixture|/srv/private|sswörd|emby p.ssword|private@example'
if grep -nE "$padrao" "$NV_SRV_LOG"; then
  echo 'credential or private path in a personal-server log' >&2
  exit 1
fi
echo "servidores_log: $linhas log lines, no token"
