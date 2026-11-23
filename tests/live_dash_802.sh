#!/bin/bash
# #802: live DASH manifest passthrough in LivePlaylistDataSource
set -eu
cd "$(dirname "$0")/.."
grep -q 'application/dash+xml' android/app/src/main/java/space/nuvio/nativelegacy/LivePlaylistDataSource.kt
grep -q 'isDashManifest' android/app/src/main/java/space/nuvio/nativelegacy/LivePlaylistDataSource.kt
grep -q '\.mpd' android/app/src/main/java/space/nuvio/nativelegacy/LivePlaylistDataSource.kt
echo "live_dash_802: PASS"
