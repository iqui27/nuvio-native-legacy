#!/bin/bash
# Real ARM32 compiler/runtime proof on the selected SDK; no credentials or TV.
set -euo pipefail
cd "$(dirname "$0")/.."
source tools/ctr.sh
source tools/tpk/sdk.sh
ctr_check_system
ctr_image_exists "$TPK_IMAGE"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cat > "$T/probe.c" <<'C'
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <pthread.h>
#include <zlib.h>
static _Thread_local int value;
static void *worker(void *unused) { (void)unused; value = 7; return (void *)(long)value; }
int probe(void) {
    pthread_t thread;
    void *result;
    value = 42;
    if (pthread_create(&thread, 0, worker, 0) || pthread_join(thread, &result)) return 1;
    return value != 42 || (long)result != 7 || zlibVersion()[0] != '1' ||
           glGetError() != 0 || !eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
}
C
cat > "$T/main.c" <<'C'
#include <dlfcn.h>
int main(void) {
    void *library = dlopen("./probe.so", RTLD_NOW);
    if (!library) return 2;
    int (*probe)(void) = dlsym(library, "probe");
    return !probe || probe();
}
C
ctr_run --rm --platform "$TPK_PLATFORM" -v "$T:/proof" -w /proof "$TPK_IMAGE" sh -ec '
  gcc $CFLAGS -shared probe.c -o probe.so -Wl,--no-undefined -Wl,--hash-style=both \
    /usr/lib/arm-linux-gnueabi/libz.a -pthread -lEGL -lGLESv2
  gcc $CFLAGS main.c -o main -ldl
  readelf -h probe.so | grep "Machine:.*ARM"
  readelf -A probe.so | grep "Tag_CPU_arch: v7"
  if readelf -A probe.so | grep -q "Tag_ABI_VFP_args: VFP registers"; then exit 1; fi
  readelf -lW probe.so | grep TLS
  readelf -dW probe.so | grep "(HASH)"
  if objdump -T probe.so | grep -qE "GLIBC_2\.(29|[3-9][0-9])"; then exit 1; fi
  '"${TPK_EXEC[*]}"' ./main
'
echo 'PASS: ARMv7 softfp shared library, glibc <= 2.28, TLS threads and dlopen'
