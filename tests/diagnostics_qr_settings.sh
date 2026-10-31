#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_dir=$(mktemp -d);trap 'rm -rf "$task_dir"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2 SDL2_image SDL2_ttf gl zlib)"
cc -std=gnu11 -D_GNU_SOURCE -DNV_WEBOS -O0 -g -ffunction-sections -fdata-sections \
 -Isrc tests/diagnostics_qr_settings.c "${flags[@]}" -ldl -pthread -lm \
 -Wl,--gc-sections -o "$task_dir/test"
"$task_dir/test"
