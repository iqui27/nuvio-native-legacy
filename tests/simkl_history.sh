#!/bin/bash
# C11/C12: real social profile path, delayed HTTP, activity gate and disk cache.
set -euo pipefail
cd "$(dirname "$0")/.."
task_tmp="$(mktemp -d "${TMPDIR:-/tmp}/nuvio-simkl-history-XXXXXX")"
trap 'if [ "${HISTORY_KEEP:-0}" != 1 ]; then rm -rf "$task_tmp"; fi' EXIT
if [ "${HISTORY_KEEP:-0}" = 1 ]; then echo "history artifacts: $task_tmp"; fi
flags=(-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Isrc -Wno-deprecated-declarations)
cc "${flags[@]}" -Dtime=history_time -Drede_baixar_st_retry=history_get \
  -Dnuvem_simkl_cliente=history_client -Ddados_gravar=history_save \
  src/simkl.c -c -o "$task_tmp/simkl.o"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/simkl.c) ;; *) sources+=("$source");; esac
done
cc "${flags[@]}" "${sources[@]}" tests/simkl_history.c "$task_tmp/simkl.o" \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -o "$task_tmp/test"
export ASAN_OPTIONS=detect_leaks=0
export DYLD_LIBRARY_PATH=/opt/homebrew/lib
export UBSAN_OPTIONS=halt_on_error=1
rc=0
for mode in async gate restore memory null stale unlinked large; do
  mkdir -p "$task_tmp/$mode"
  # gate and restore deliberately share storage, but run in fresh processes.
  data="$task_tmp/$mode"
  if [ "$mode" = restore ]; then data="$task_tmp/gate"; fi
  NUVIO_DADOS="$data" "$task_tmp/test" "$mode" || rc=1
done
exit "$rc"
