#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
dir=$(mktemp -d);trap 'rm -rf "$dir"' EXIT
read -r -a flags <<< "$(pkg-config --cflags --libs sdl2)"
for mode in enabled disabled no-converter samsung;do
 extra=();case "$mode" in
 enabled)extra=(-DNV_WEBOS -DNV_DV7_CONVERSION -DNV_DV7_MAPPING_TEST);;
 disabled)extra=(-DNV_WEBOS -DNV_DV7_CONVERSION);;
 no-converter)extra=(-DNV_WEBOS -DNV_DV7_MAPPING_TEST);;
 samsung)extra=(-DNV_TPK -DNV_DV7_CONVERSION -DNV_DV7_MAPPING_TEST);;
 esac
 cc -std=gnu11 -D_GNU_SOURCE "${extra[@]}" -O0 -g -ffunction-sections -fdata-sections \
  -Isrc tests/dv7_mapping_settings.c "${flags[@]}" -lSDL2_image -lSDL2_ttf -lGL -lz -ldl -pthread -lm -Wl,--gc-sections -o "$dir/$mode"
 "$dir/$mode"
done
