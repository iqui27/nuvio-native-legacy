#!/bin/bash
# Real-ARM benchmark of four libass builds (libass-hybrid-test branch only).
#   stock  libass 0.17.5
#   ours   0.17.5 + tools/tpk/libass-nv/patches (what the .tpk ships today)
#   nova   Laskco/mpvNova-libass @ NOVA_REV (NuvioMedia/NuvioTV PR 4799), -DASS_EVENT_THREADS
#   hyb    nova + tools/tpk/libass-hybrid/hybrid.patch
# For each arch (arm64 native; armhf native when the CPU runs AArch32, else
# skipped: qemu timings are not CPU timings) it renders the libass-nv samples
# plus a synthetic heavy track (gen_heavy.py) on a 24 fps grid at 1080p with
# bench.c, hashes every frame (byte compare against stock) and runs verify.c
# on the hybrid. Usage: run.sh <repo root> <work dir>
set -e
R=$(cd "$1" && pwd); W=$2; mkdir -p "$W"; W=$(cd "$W" && pwd)
NOVA_REV=${NOVA_REV:-b6209274415490615baa3f01f2409cb5facda674}
F=$R/deploy/app/fonts; B=$R/tools/tpk/libass-hybrid/bench
OUT=$W/results.md; : > "$OUT"
log(){ echo "$*" | tee -a "$OUT"; }

cd "$W"
[ -d libass ] || git clone -q https://github.com/libass/libass.git
[ -d nova ] || git clone -q https://github.com/Laskco/mpvNova-libass.git nova
rm -rf src; mkdir src
git -C libass worktree add -q --detach -f "$W/src/stock" 0.17.5
git -C libass worktree add -q --detach -f "$W/src/ours" 0.17.5
for p in "$R"/tools/tpk/libass-nv/patches/*.patch; do patch -d src/ours -p1 -s < "$p"; done
git -C nova worktree add -q --detach -f "$W/src/nova" "$NOVA_REV"
git -C nova worktree add -q --detach -f "$W/src/hyb" "$NOVA_REV"
patch -d src/hyb -p1 -s < "$R/tools/tpk/libass-hybrid/hybrid.patch"
mkdir -p samples; cp "$R"/tools/tpk/libass-nv/samples/*.ass samples/; python3 "$B/gen_heavy.py" > samples/heavy.ass; python3 "$B/gen_fx.py" > samples/fx.ass
hdr(){ sed -n "1,/^Format: Layer/p" samples/heavy.ass; }
{ hdr; grep "^Dialogue" samples/heavy.ass | grep "clip(m"; } > samples/clipwipe.ass
{ hdr; grep "^Dialogue" samples/heavy.ass | grep "Sign,,0,0,0,,{\\\\pos"; } > samples/signs.ass

cat > armhf.ini <<'X'
[binaries]
c = 'arm-linux-gnueabihf-gcc'
cpp = 'arm-linux-gnueabihf-g++'
ar = 'arm-linux-gnueabihf-ar'
strip = 'arm-linux-gnueabihf-strip'
pkg-config = 'pkg-config'
[properties]
pkg_config_libdir = '/usr/lib/arm-linux-gnueabihf/pkgconfig:/usr/share/pkgconfig'
needs_exe_wrapper = true
[host_machine]
system = 'linux'
cpu_family = 'arm'
cpu = 'armv7'
endian = 'little'
X

build(){ # build <variant> <arch>
  v=$1; a=$2; b=$W/build/$v-$a; rm -rf "$b"; mkdir -p "$W/build"
  case $a in
    arm64) cross=; cf="-O3"; [ "$(uname -m)" = aarch64 ] && cf="-O3 -mtune=cortex-a53"; cc=gcc; ml=$(gcc -dumpmachine);;
    armhf) cross="--cross-file $W/armhf.ini"; cf="-O3 -mtune=cortex-a53 -march=armv7-a -mfpu=neon -mfloat-abi=hard"; cc="arm-linux-gnueabihf-gcc -march=armv7-a -mfpu=neon -mfloat-abi=hard"; ml=arm-linux-gnueabihf;;
  esac
  case $v in nova|hyb) cf="$cf -DASS_EVENT_THREADS";; esac
  meson setup "$b" "$W/src/$v" $cross --buildtype=plain -Ddefault_library=static -Dfontconfig=disabled \
    -Drequire-system-font-provider=false -Dtest=disabled -Dcompare=disabled -Dprofile=disabled -Dfuzz=disabled \
    -Dcheckasm=disabled -Dlibunibreak=disabled -Dc_args="$cf" > "$b.log" 2>&1 || { tail -30 "$b.log"; return 1; }
  ninja -C "$b" >> "$b.log" 2>&1 || { tail -40 "$b.log"; return 1; }
  mkdir -p "$b/inc/ass"; cp "$W/src/$v/libass/ass.h" "$W/src/$v/libass/ass_types.h" "$b/inc/ass/"
  nv=; grep -q ass_nv_set_threads "$W/src/$v/libass/ass.h" && nv=-DHAVE_NV
  L="$b/libass/libass.a -L/usr/lib/$ml -lharfbuzz -lfribidi -lfreetype -lm -lpthread"
  $cc -O2 $nv -I"$b/inc" -o "$b/bench" "$B/bench.c" $L
  [ -z "$nv" ] || $cc -O2 -I"$b/inc" -o "$b/verify" "$R/tools/tpk/libass-nv/verify.c" $L
}

# run <label> <variant> <arch> [env...]
run(){
  label=$1; v=$2; a=$3; shift 3
  env "$@" REPS=${REPS:-3} taskset -c 0-3 "$W/build/$v-$a/bench" "$F" "$W/h-$label.txt" samples/*.ass > "$W/r-$label.txt"
  cells=$(awk '/frames/{printf " %s / %s |", $7, $14} /^TOTAL/{printf " %s |", $2}' "$W/r-$label.txt")
  same=$(join <(awk '{print $1":"$2, $3}' "$W/h-stock-$a.txt" | sort) <(awk '{print $1":"$2, $3}' "$W/h-$label.txt" | sort) | awk '$2!=$3{d++} END{print d+0}')
  log "| $label | $* |$cells $same |"
}

table(){
  log ""; log "### $1 (each cell: total ms over the track / p95 ms per frame; best of ${REPS:-3})"; log ""
  h="| run | env |"; d="|---|---|"
  for f in samples/*.ass; do h="$h $(basename "$f" .ass) |"; d="$d---|"; done
  log "$h all ms | frames differing from stock |"; log "$d---|---|"
}

log "## libass bench on $(uname -m), $(nproc) CPUs: $(grep -m1 -E 'model name|CPU part' /proc/cpuinfo | sed 's/.*: //')"
log "lscpu: $(lscpu | grep -E 'Model name' | head -1 | sed 's/  */ /g')"
for a in arm64 armhf; do
  if [ $a = armhf ]; then
    echo 'int main(void){return 0;}' > t.c; arm-linux-gnueabihf-gcc -o t32 t.c 2>/dev/null
    if ! ./t32 2>/dev/null; then log ""; log "armhf: this CPU does not run AArch32 code natively; skipped (qemu would not give CPU timings)."; continue; fi
  fi
  for v in stock ours nova hyb; do build $v $a; done
  table "$a"
  run stock-$a stock $a
  if [ $a = armhf ]; then
    run ours-$a-t1 ours $a NVTHREADS=1
    run ours-$a-t3 ours $a NVTHREADS=3
  else
    run ours-$a ours $a
  fi
  run nova-$a-serial nova $a LIBASS_RENDER_THREADS=1
  run nova-$a-default nova $a
  run nova-$a-edges1 nova $a LIBASS_EXPERIMENTAL_EDGES=1
  if [ $a = armhf ]; then
    run hyb-$a-serial hyb $a LIBASS_RENDER_THREADS=1 NVTHREADS=3
    run hyb-$a-default hyb $a NVTHREADS=3
    run hyb-$a-novaengine hyb $a ASS_NV_ARMV7=nova
    run hyb-$a-default-edges1 hyb $a NVTHREADS=3 LIBASS_EXPERIMENTAL_EDGES=1
  else
    run hyb-$a-serial hyb $a LIBASS_RENDER_THREADS=1
    run hyb-$a-default hyb $a
  fi
  log ""
  log "verify.c on hyb-$a (C vs SIMD, 1/4 blur threads, opacity, split): $(taskset -c 0-3 "$W/build/hyb-$a/verify" "$F" "$R"/tools/tpk/libass-nv/samples/*.ass 2>&1 | grep -E 'RESULT|FAIL|differ' | grep -v ' 0 differ' | tr '\n' ' ')"
done
cat "$OUT"
