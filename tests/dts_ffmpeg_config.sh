#!/bin/sh
# Exercise the build guard with old, incomplete and supported SDK configurations.
set -eu
cd "$(dirname "$0")/.."
dir=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-dts-config.XXXXXX")
trap 'rm -rf "$dir"' EXIT
reject() {
  if sh tools/check-dts-ffmpeg.sh "$dir" >"$dir/log" 2>&1; then
    echo "Unsupported SDK accepted" >&2; exit 1
  fi
  grep -q 'rebuild tools/Dockerfile' "$dir/log"
  if [ "$#" -gt 0 ]; then grep -q "confirmed $1;" "$dir/log"; fi
}
reject # An old SDK has headers/libraries but no component record.
for feature in DCA_DECODER TRUEHD_DECODER AC3_DECODER EAC3_DECODER AAC_ENCODER AC3_ENCODER; do
  printf 'CONFIG_%s=yes\n' "$feature"
done > "$dir/nuvio-audio-config"
cp "$dir/nuvio-audio-config" "$dir/supported"
for feature in DCA_DECODER TRUEHD_DECODER AC3_DECODER EAC3_DECODER AAC_ENCODER AC3_ENCODER; do
  grep -v "^CONFIG_$feature=" "$dir/supported" > "$dir/nuvio-audio-config"
  reject "$feature"
  printf 'CONFIG_%s=no\n' "$feature" >> "$dir/nuvio-audio-config"
  reject "$feature"
done
cp "$dir/supported" "$dir/nuvio-audio-config"
sh tools/check-dts-ffmpeg.sh "$dir"
echo 'dts_ffmpeg_config: old/incomplete SDK rejected; supported component config accepted'
