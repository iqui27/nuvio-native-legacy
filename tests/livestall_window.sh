#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-livestall-window.XXXXXX")
trap 'rm -rf "$work"' EXIT
flags=(-O1 -g -Wall -Wextra -Werror)
if [ "${SANITIZE:-0}" = 1 ]; then flags+=(-fsanitize=address,undefined -fno-omit-frame-pointer); fi
cc "${flags[@]}" tests/livestall_window.c -o "$work/test"
"$work/test"
cc "${flags[@]}" -DNV_TPK tests/livestall_window.c -o "$work/tpk"
"$work/tpk"
cc "${flags[@]}" -DNV_ANDROID tests/livestall_window.c -o "$work/android"
"$work/android"
