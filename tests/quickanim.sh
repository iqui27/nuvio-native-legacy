#!/bin/bash
# Real GL regression: integration must fail, current sources must pass.
set -euo pipefail
cd "$(dirname "$0")/.."
out=/Volumes/ExternalSSD/tmp/rev/quickanim
mkdir -p "$out"
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio-quickanim.XXXXXX")
trap 'rm -rf "$work"' EXIT
base=$(git rev-parse integracao/2.1)
git show "$base:src/central.c" > "$work/central.c"
git show "$base:src/ilha.c" > "$work/ilha.c"
# Preserve central.c's relative includes when included from the temporary path.
cp src/*.h "$work/"
sources=()
for source in src/*.c src/dts/*.c; do
  case "$source" in src/main.c|src/central.c|src/ilha.c) ;; *) sources+=("$source");; esac
done
compile() {
  cc "${sources[@]}" "$1" tests/quickanim.c -Isrc -o "$work/test" -DCENTRAL_TESTE \
    "-DQUICKANIM_CENTRAL=\"$2\"" \
    -O1 -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
    -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
    -Wno-deprecated-declarations -Wno-macro-redefined
}
mkdir -p "$work/before" "$work/after"
compile "$work/ilha.c" "$work/central.c"
set +e
NUVIO_DADOS="$work/before" "$work/test" "$out/before.png" > "$out/before.log" 2>&1
rc=$?
set -e
if [ "$rc" != 1 ]; then echo "Expected integration regression failure, got $rc"; exit 1; fi
if ! grep -q 'FAIL: no content pixels' "$out/before.log" || ! grep -q 'FAIL: content alpha' "$out/before.log"; then
  echo 'Integration failed for an unexpected reason'; exit 1
fi
printf 'PASS: integracao/2.1 %s reproduces opacity and clipping failures\n' "$base"
compile src/ilha.c "$PWD/src/central.c"
NUVIO_DADOS="$work/after" "$work/test" "$out/after.png" | tee "$out/after.log"
