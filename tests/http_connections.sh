#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
prefix=${NUVIO_DTS_ROOT:-/tmp/nuvio-dts-host}
task_dir=$(mktemp -d);trap 'rm -rf "$task_dir"' EXIT
flags=(-std=gnu11 -O1 -g -DNV_DTS_FFMPEG -DNV_DTS_WEBOS_RECOVERY -Wall -Wextra -Werror -Wno-misleading-indentation -Isrc -I"$prefix/include")
if [[ ${SANITIZE:-0} == 1 ]];then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer);fi
cc "${flags[@]}" tests/http_connections.c src/rede.c src/redeurl.c -o "$task_dir/test" -L"$prefix/lib" -Wl,--start-group -lavformat -lavcodec -lswresample -lavutil -Wl,--end-group -lm -pthread -ldl
python3 tests/http_connections.py "$task_dir/test"
