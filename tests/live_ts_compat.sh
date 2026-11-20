#!/bin/bash
set -eu
cd "$(dirname "$0")/.."
grep -qF 'Regex("(?i)^/live/[^/]+/[^/]+/[^/]+\\.ts$")' android/app/src/main/java/space/nuvio/nativelegacy/LivePlaylistDataSource.kt
grep -q 'APPLICATION_MPD' android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
echo live_ts_compat: PASS
