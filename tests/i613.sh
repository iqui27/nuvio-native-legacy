#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-i613.XXXXXX")
trap 'if [ -f "$work/log" ]; then pid=$(awk -F= "/^server pid=/{print \$2}" "$work/log"); [ -z "$pid" ] || kill "$pid" 2>/dev/null || true; fi; rm -rf "$work"' EXIT
flags=(-O1 -g -Isrc -Wno-deprecated-declarations -Wno-macro-redefined -Wno-return-type -ffunction-sections -fdata-sections)
if [ "$(uname -s)" = Darwin ]; then
  jdk="${JAVA_HOME:-$(/usr/libexec/java_home)}"
  flags+=(-I"$jdk/include" -I"$jdk/include/darwin" -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 -Wl,-dead_strip)
else
  jdk="${JAVA_HOME:?Set JAVA_HOME to a JDK containing JNI headers}"
  read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2)"
  flags+=(-I"$jdk/include" -I"$jdk/include/linux" "${sdl_flags[@]}" -Wl,--gc-sections)
fi
cc "${flags[@]}" tests/i613.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread -lm -o "$work/test"
run() {
  set +e
  "$@" 2>&1 | tee "$work/log"
  rc=${PIPESTATUS[0]}
  set -e
  [ "$rc" = 0 ] || exit "$rc"
}
run "$work/test"
cc "${flags[@]}" -DI613_STREAMS -DNV_LINUX_DESKTOP tests/i613.c \
  src/fontefalha.c src/fonteauto.c src/limpa.c src/dobra.c src/fontenota.c src/linguas.c src/fontefiltro.c \
  -lpthread -lm -o "$work/streams"
run "$work/streams"
cc "${flags[@]}" -DI613_APP -DNV_LINUX_DESKTOP tests/i613.c \
  src/fontefalha.c src/fontefiltro.c src/linguas.c src/fonteantecipa.c src/inicio.c src/fontesonda.c \
  -lpthread -lm -o "$work/app"
run "$work/app"
cc "${flags[@]}" -DNV_LINUX_DESKTOP tests/fontefalha_playback.c \
  src/fontefalha.c src/fontefiltro.c src/linguas.c src/fonteantecipa.c src/inicio.c src/fontesonda.c \
  -lpthread -lm -o "$work/failover"
run "$work/failover" "$work"
# Existing JNI resume/pause/live regressions use the same compiled boundaries.
cc "${flags[@]}" tests/video_android_retomada.c src/capmkv.c src/mkv.c tests/capmkv_stub.c -lpthread -lm -o "$work/resume"
printf '3\n' > "$work/paused-ack"
run "$work/resume" "$work/paused-ack"
