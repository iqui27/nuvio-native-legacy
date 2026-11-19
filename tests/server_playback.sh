#!/bin/bash
# One offline server module target: request contracts, legacy migration and state.
set -euo pipefail
cd "$(dirname "$0")/.."
if [ "${1:-}" = --baseline ]; then
  work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-server-playback-before.XXXXXX")
  trap 'rm -rf "$work"' EXIT
  git archive fab11c7bf src tests | tar -x -C "$work"
  cp tests/jellyfin.c tests/jellyfin_server.py tests/server_playback.inc "$work/tests/"
  if (cd "$work" && bash tests/jellyfin.sh) > "$work/log" 2>&1; then
    echo "FAIL: baseline unexpectedly accepted playback preferences" >&2; exit 1
  fi
  grep -F 'pb->n == 1' "$work/log" >/dev/null
  echo "PASS: baseline fails Direct Play-only migration assertion"
  exit 0
fi
bash tests/jellyfin.sh
bash tests/emby.sh
bash tests/plex.sh
python3 tools/idiomas.py
