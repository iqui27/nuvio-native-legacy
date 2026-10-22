/* LG webOS: keeps the audio sink of an in-process Starfish BUFFERSTREAM
 * pipeline from crashing the app.
 *
 * Measured from a core dump on an LG C9 (webOS 4.10.2), with AAC and with
 * E-AC-3: SIGSEGV in pthread_setname_np+0x48 <- KADP_OSA_CreateThread+0x77
 * <- KADP_AUDIO_OpenMaster <- lxao_open_renderer <- gstlxaudiosink.
 * KADP_OSA_CreateThread asks for SCHED_RR priority 60 with a 24576-byte stack;
 * inside this process pthread_create answers EINVAL (22), the firmware only
 * checks for "< 0" and then names a garbage pthread_t. Why EINVAL here and not
 * in LG's own media process is not proven (the app's static TLS may not fit
 * that stack). Retrying with default attributes (keeping the detach state and
 * any stack of at least 128 KiB) starts the thread and playback proceeds with
 * sound, in sync, on that TV.
 *
 * Interposition: tools/arm.sh exports this pthread_create from the executable
 * (-Wl,--export-dynamic-symbol=pthread_create), so the dynamic linker binds
 * the firmware libraries to it before libpthread. Successful calls are not
 * altered in any way. */
#if defined(NV_WEBOS) && !defined(NV_TPK) && !defined(NV_ANDROID)
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>

typedef int (*NvCreate)(pthread_t *, const pthread_attr_t *, void *(*)(void *), void *);

int pthread_create(pthread_t *t, const pthread_attr_t *attr, void *(*fn)(void *), void *arg) {
  static NvCreate real;
  static int logged;
  int r;
  if (!real) real = (NvCreate)dlsym(RTLD_NEXT, "pthread_create");
  if (!real) return EAGAIN;
  r = real(t, attr, fn, arg);
  if (r && attr) {
    int first = r, policy = -1, detach = 0;
    size_t stack = 0;
    pthread_attr_t clean;
    pthread_attr_getschedpolicy(attr, &policy);
    pthread_attr_getstacksize(attr, &stack);
    pthread_attr_getdetachstate(attr, &detach);
    if (pthread_attr_init(&clean)) return first;
    pthread_attr_setdetachstate(&clean, detach);
    if (stack >= 128 * 1024) pthread_attr_setstacksize(&clean, stack);
    r = real(t, &clean, fn, arg);
    pthread_attr_destroy(&clean);
    if (logged < 4) {
      logged++;
      printf("[rt-shim] pthread_create failed %d (policy=%d stack=%lu): retried with default attributes -> %d\n",
             first, policy, (unsigned long)stack, r);
      fflush(stdout);
    }
  }
  return r;
}
#else
typedef int nv_rt_shim_unused;
#endif
