/* Exercise the real webOS crash handler with maps beyond the old 24 KiB
 * cutoff. No firmware or network is needed; only /proc/self/maps is stubbed. */
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ucontext.h>

static const char *maps_path;
static int fixture_open(const char *path, int flags, ...) {
  if (!strcmp(path, "/proc/self/maps")) return open(maps_path, flags);
  return open(path, flags, 0644);
}
#define open fixture_open
#include "../src/queda.c"
#undef open

/* Simulate LG's EINVAL, then verify the existing retry still preserves detach
 * state and publishes diagnostics before returning to the firmware. */
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
static int creates;
static int fake_create(pthread_t *t, const pthread_attr_t *attr,
                       void *(*fn)(void *), void *arg) {
  (void)t; (void)fn; (void)arg;
  if (++creates == 1) return EINVAL;
  int detach = -1, policy = -1;
  size_t stack = 0;
  assert(!pthread_attr_getdetachstate(attr, &detach) && detach == PTHREAD_CREATE_DETACHED);
  assert(!pthread_attr_getschedpolicy(attr, &policy) && policy == SCHED_OTHER);
  assert(!pthread_attr_getstacksize(attr, &stack) && stack >= 128 * 1024);
  return 0;
}
static void *fake_dlsym(void *handle, const char *name) {
  (void)handle;
  assert(!strcmp(name, "pthread_create"));
  return (void *)fake_create;
}
#define NV_WEBOS
#define dlsym fake_dlsym
#define pthread_create shim_create
#include "../src/dts/rt_shim.c"
#undef pthread_create
#undef dlsym

int main(int argc, char **argv) {
  assert(argc == 2);
  char maps[512], report[512], output[4096] = "";
  snprintf(maps, sizeof maps, "%s/maps", argv[1]);
  snprintf(report, sizeof report, "%s/report", argv[1]);
  maps_path = maps;
  FILE *f = fopen(maps, "w");
  assert(f);
  /* More than both old limits: 24 KiB captured / 512 maps parsed. */
  for (unsigned i = 0; i < 700; i++)
    fprintf(f, "%08x-%08x r--p 00000000 00:00 0 /irrelevant/map-%u.so\n",
            0x10000 + i * 0x1000, 0x11000 + i * 0x1000, i);
  fputs("e7f00000-e7f50000 r--p 00000000 00:00 0 /lib/libpthread.so.0\n"
        "e7f50000-e8000000 r-xp 00050000 00:00 0 /lib/libpthread.so.0\n", f);
  fclose(f);

  ucontext_t ctx = {0};
  struct __darwin_mcontext64 machine = {0};
  ctx.uc_mcontext = &machine;
  machine.__ss.__pc = 0xe7f572a2;
  machine.__ss.__lr = 0xe7f57300;
  siginfo_t info = {0};
  info.si_code = 1; info.si_addr = (void *)(uintptr_t)0x8a54;
  snprintf(qdArq, sizeof qdArq, "%s", report);
  qdTratar(SIGSEGV, &info, &ctx);
  f = tmpfile();
  assert(f && queda_relatar_em(report, f));
  rewind(f);
  fread(output, 1, sizeof output - 1, f);
  fclose(f);
  assert(strstr(output, "pc libpthread.so.0+0x572a2"));
  assert(strstr(output, "lr libpthread.so.0+0x57300"));
  assert(strstr(output, "[queda] fio "));
  assert(!strstr(output, "fora de qualquer modulo"));
  pthread_attr_t attr;
  pthread_t thread;
  assert(!pthread_attr_init(&attr));
  assert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  assert(!pthread_attr_setstacksize(&attr, 128 * 1024));
  f = tmpfile();
  assert(f);
  fflush(stdout);
  int saved = dup(STDOUT_FILENO);
  assert(saved >= 0 && dup2(fileno(f), STDOUT_FILENO) >= 0);
  assert(!shim_create(&thread, &attr, NULL, NULL) && creates == 2);
  fflush(stdout);
  assert(dup2(saved, STDOUT_FILENO) >= 0);
  close(saved);
  rewind(f);
  memset(output, 0, sizeof output);
  fread(output, 1, sizeof output - 1, f);
  fclose(f);
  pthread_attr_destroy(&attr);
  assert(strstr(output, "[rt-shim] active real=resolved"));
  assert(strstr(output, "; retrying\n") && strstr(output, "attributes -> 0"));
  puts("c9_queda: late module PC/LR, thread identity and shim retry diagnostics preserved");
  return 0;
}
