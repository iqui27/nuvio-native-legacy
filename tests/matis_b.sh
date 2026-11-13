#!/usr/bin/env bash
# Offline regression suite for the Matis94 B backport; no account or device access.
set -euo pipefail
cd "$(dirname "$0")/.."
export PATH="/Users/hrocha/.rustup/toolchains/stable-aarch64-apple-darwin/bin:$PATH"
for test in audioescolha audiomemoria video_faixa_rotulo video_audio_mkv hevc_annexb dts_playback dts_pgs dv7_engine mp4_bd_engine dvaudio_lg dv7_settings; do
  printf '\nRunning bash tests/%s.sh\n' "$test"
  bash "tests/$test.sh"
done
