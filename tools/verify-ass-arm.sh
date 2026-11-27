#!/bin/bash
# The webOS .ipk's libass (tools/build-ass-arm.sh: mpvNova-libass + our
# tools/tpk/libass-hybrid patch) checked on armv7, the same checks the .tpk's
# tools/tpk/libass-nv/verify.sh runs:
#  1. libass's checkasm with the armv7 NEON entries (both engines: ours and
#     mpvNova's), several seeds, threaded paths on the odd ones;
#  2. verify.c: real renders of tools/tpk/libass-nv/samples/*.ass, C engine vs
#     NEON (1 and 4 blur threads) byte for byte, plus dialogue opacity.
# Built with the webOS cross compiler inside the SDK image, run under
# qemu-arm. Linked -static: under qemu-user 8.2 the SDK's dynamic loader
# crashes on a plain "return 0" program, a static binary runs fine.
#
#   bash tools/verify-ass-arm.sh        # needs qemu-arm-static on the host
# NUVIO_BUILD_PLATFORM / NUVIO_SDK_IMAGE as in tools/arm.sh.
set -euo pipefail
cd "$(dirname "$0")/.." && source tools/ctr.sh
BUILD_PLATFORM="${NUVIO_BUILD_PLATFORM:-linux/arm64}"
SDK_IMAGE="${NUVIO_SDK_IMAGE:-nuvio-webos-sdk}"
QEMU="${NUVIO_QEMU_ARM:-$(command -v qemu-arm-static || command -v qemu-arm || true)}"
[ -n "$QEMU" ] || { echo "verify-ass-arm: qemu-arm-static missing (apt install qemu-user-static)" >&2; exit 2; }
ctr_run --rm --platform "$BUILD_PLATFORM" -v "$PWD":/work -v "$QEMU":/usr/local/bin/qemu-arm:ro "$SDK_IMAGE" bash -c '
  set -euo pipefail
  P=$NUVIO_ASS_ROOT; S=/tmp/nuvio-ass-build/libass-hybrid; SR=$NUVIO_SYSROOT
  T=/tmp/verify; mkdir -p "$T"
  [ -f "$P/lib/.libass-hybrid" ] && [ -d "$S/checkasm" ] || { echo "verify-ass-arm: the image has no hybrid libass build tree (rebuild tools/Dockerfile)" >&2; exit 1; }
  CC=arm-webos-linux-gnueabi-gcc
  CF="-O2 $(tr -d "\n" < /work/tools/tpk/libass-hybrid/cflags)"
  echo "[verify] webOS libass hybrid $(cut -c1-12 "$P/lib/.libass-hybrid") | $($CC -dumpversion) | $CF"
  printf "#define CONFIG_ASM 0\n#define CONFIG_LARGE_TILES 0\n#define CONFIG_SOURCEVERSION \"nv\"\n" > "$T/config.h"
  LIBS="$P/lib/libass.a $P/lib/libharfbuzz.a $P/lib/libfribidi.a $P/lib/libfreetype.a -lstdc++ -lm -lpthread"
  for f in "$S"/checkasm/*.c; do
    $CC $CF -std=gnu99 -I"$T" -I"$S/libass" -I"$S" -I"$P/include" -I"$P/include/freetype2" -c "$f" -o "$T/$(basename "$f" .c).o"
  done
  $CC -static -o "$T/checkasm" "$T"/*.o $LIBS
  $CC $CF -static -I"$P/include" -o "$T/verify" /work/tools/tpk/libass-nv/verify.c $LIBS
  for eng in nova nv; do
    for seed in 1 22 333; do
      if [ $((seed % 2)) = 1 ]; then TH="ASS_NV_THREADS=4 ASS_NV_PAR_MIN=0"; else TH=; fi
      if ! env ASS_NV_ARMV7=$eng $TH qemu-arm "$T/checkasm" $seed > "$T/checkasm.txt" 2>&1; then cat "$T/checkasm.txt"; exit 1; fi
      grep -q "NEON (armv7)" "$T/checkasm.txt" || { cat "$T/checkasm.txt"; echo "[verify] checkasm did not test the NEON engine" >&2; exit 1; }
      echo "[verify] checkasm engine=$eng seed $seed ${TH:-1 thread}: $(tail -n 1 "$T/checkasm.txt")"
    done
    ASS_NV_ARMV7=$eng qemu-arm "$T/verify" /work/deploy/app/fonts /work/tools/tpk/libass-nv/samples/*.ass | sed "s/^/[verify $eng] /"
  done
'
