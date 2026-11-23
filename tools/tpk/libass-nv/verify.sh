#!/bin/sh
# CI only (.github/workflows/build-tpk.yml, step "Verify libass NEON engine"):
# runs INSIDE the TPK build image (tools/tpk/Dockerfile, linux/arm under
# qemu), with the deps cache at /w and the repo at /work, after tools/tpk.sh
# built the patched libass (tools/tpk/libass-nv/patches) into /w/prefix.
#
#  1. libass's own checkasm, with the armv7 NEON entry the patch adds: every
#     engine function (rasterizer, blend, be_blur, blur stripes) against the C
#     engine on random input, several seeds.
#  2. verify.c: real libass renders of samples/*.ass on a time grid, C engine
#     vs NEON engine (1 and 4 blur threads), every ASS_Image compared byte for
#     byte, plus the dialogue opacity (alpha only, signs untouched).
#
# Exits non-zero on any difference, which fails the build.
set -e
P=/w/prefix
S=/w/src/libass-nv
T="${TMPDIR:-/tmp}/libass-nv-verify"
rm -rf "$T"; mkdir -p "$T"
[ -f "$P/lib/libass.a" ] && [ -d "$S/checkasm" ] || { echo "verify: patched libass not built (run tools/tpk.sh first)" >&2; exit 1; }
echo "[verify] libass $(cat "$P/lib/.libass-nv" 2>/dev/null | cut -c1-12) | gcc $(gcc -dumpversion) | CFLAGS=$CFLAGS"

# checkasm only needs these three from config.h; the rest of libass is in libass.a.
printf '#define CONFIG_ASM 0\n#define CONFIG_LARGE_TILES 0\n#define CONFIG_SOURCEVERSION "nv"\n' > "$T/config.h"
LIBS="$P/lib/libass.a $P/lib/libharfbuzz.a $P/lib/libfribidi.a $P/lib/libfreetype.a -lstdc++ -lm -lpthread"
for f in "$S"/checkasm/*.c; do
  gcc $CFLAGS -std=gnu99 -I"$T" -I"$S/libass" -I"$S" -I"$P/include" -I"$P/include/freetype2" \
    -c "$f" -o "$T/$(basename "$f" .c).o"
done
gcc -o "$T/checkasm" "$T"/*.o $LIBS
for seed in 1 22 333 4444 55555; do
  # odd seeds: the NEON engine with 4 threads and every job split (threaded paths)
  if [ $((seed % 2)) = 1 ]; then TH="ASS_NV_THREADS=4 ASS_NV_PAR_MIN=0"; else TH=; fi
  if ! env $TH "$T/checkasm" $seed > "$T/checkasm.txt" 2>&1; then cat "$T/checkasm.txt"; exit 1; fi
  grep -q "NEON (armv7)" "$T/checkasm.txt" || { cat "$T/checkasm.txt"; echo "[verify] checkasm did not test the NEON engine" >&2; exit 1; }
  echo "[verify] checkasm seed $seed ${TH:-1 thread}: $(tail -n 1 "$T/checkasm.txt")"
done
cat "$T/checkasm.txt"

gcc $CFLAGS -I"$P/include" -o "$T/verify" /work/tools/tpk/libass-nv/verify.c $LIBS
"$T/verify" /work/deploy/app/fonts /work/tools/tpk/libass-nv/samples/*.ass
