#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rc=$?; if [ "$rc" = 0 ]; then rm -rf "$tmp"; else echo "Failed test artifacts: $tmp"; fi' EXIT
sources=()
for f in src/*.c src/dts/*.c; do
  case "$f" in src/main.c|src/perfil.c) continue;; esac
  sources+=("$f")
done
out=/Volumes/ExternalSSD/tmp/rev/perfilstats
mkdir -p "$out" "$tmp/data"
cc -O0 -Isrc tests/perfilstats_flush.c -pthread -lm -o "$tmp/flush"
"$tmp/flush"
flags=(-DNV_TOUCH_UI -DNV_SOCIALVIS_DEMO -Isrc -O0 -w
  -I/opt/homebrew/include -I/opt/homebrew/include/SDL2)
# Compile shared dependencies once; both captures use the same renderer/data.
objects=()
for f in "${sources[@]}"; do
  o="$tmp/$(basename "${f%.c}").o"
  cc "${flags[@]}" -c "$f" -o "$o"
  objects+=("$o")
done
libs=(-L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL)
git show 00fd00ba5a37c116eaeacb8d6e81791829186c9c:src/perfil.c > "$tmp/profile-before.c"
cc "${flags[@]}" -DPF_BASELINE_FILE=\""$tmp/profile-before.c"\" tests/perfilstats.c "${objects[@]}" "${libs[@]}" -o "$tmp/before"
NUVIO_DADOS="$tmp/data" "$tmp/before" "$out/before"
cc "${flags[@]}" tests/perfilstats.c "${objects[@]}" "${libs[@]}" -o "$tmp/check"
NUVIO_DADOS="$tmp/data" "$tmp/check" "$out/after"
# Existing profile contracts share the linked production objects.
for test in perfil_esquerda_barra perfil_pilula; do
  cc "${flags[@]}" "tests/$test.c" "${objects[@]}" "${libs[@]}" -o "$tmp/$test"
  mkdir -p "$tmp/$test-data"
  NUVIO_DADOS="$tmp/$test-data" "$tmp/$test"
done
cc -O2 -Isrc tests/corviva.c src/corviva.c -lm -o "$tmp/colors"
"$tmp/colors"
