#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-c1.XXXXXX")
trap 'rm -rf "$task_tmp"' EXIT
prefix=${NUVIO_DTS_ROOT:-/opt/homebrew}
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-misleading-indentation -Wno-implicit-const-int-float-conversion)
engine_flags=()
if [[ ${1:-} == --baseline ]]; then
  # Isolated source snapshot, no checkout/stash or changes to this worktree.
  git archive 2af4aefdc78f71e3537bfb7bb06d0c4b1b719191 src | tar -x -C "$task_tmp"
  engine_flags=(-DC1_BASELINE "-DC1_ENGINE=\"$task_tmp/src/dts/dts_engine.c\"")
fi
if [[ ${SANITIZE:-0} == 1 ]]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" "${engine_flags[@]}" -I"$prefix/include" tests/c1_transport.c src/rede.c src/redeurl.c \
  -o "$task_tmp/test" -L"$prefix/lib" -lavformat -lavcodec -lswresample -lavutil -lz -lm -lpthread -ldl
ffmpeg -hide_banner -loglevel error -f lavfi -i 'color=size=64x64:rate=25:duration=2' \
  -f lavfi -i 'sine=frequency=440:duration=2' -c:v libx264 -bf 0 -g 25 \
  -c:a dca -strict -2 -b:a 768000 "$task_tmp/input.mkv"
# Large tail exercises continuity past the former 4 MiB request boundaries and
# ring wrap without an expensive video encode. Container metadata stays valid.
python3 - "$task_tmp/input.mkv" <<'PY'
import sys
with open(sys.argv[1], 'ab') as f:
    f.truncate(24*1024*1024)
PY
mode=()
if [[ ${1:-} == --baseline ]]; then mode=(baseline); fi
python3 tests/c1_transport.py "$task_tmp/test" "$task_tmp/input.mkv" "${mode[@]}"
