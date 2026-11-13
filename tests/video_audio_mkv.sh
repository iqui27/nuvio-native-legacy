#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d);trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -I/opt/homebrew/include tests/video_audio_mkv.c src/audioinfo.c src/linguas.c -o "$tmp/test"
"$tmp/test"
