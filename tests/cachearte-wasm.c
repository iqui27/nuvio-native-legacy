#include "cachearte.h"

#include <emscripten/emscripten.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *s_url = "https://images.example/art.jpg?token=opaque&rev=1";
static _Atomic int s_done, s_result;
static _Atomic int s_abort_done, s_abort_result;
static _Atomic int s_read_abort_done, s_read_abort_result;
static _Atomic int s_quota_done, s_quota_result;
static int s_stage;

static void pause_ms(long ms) {
  struct timespec t;
  t.tv_sec = ms / 1000; t.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&t, NULL);
}

static void *worker(void *unused) {
  unsigned char *bytes = NULL;
  long n = 0;
  int ok = 1;
  (void)unused;
  if (s_stage == 1) {
    if (cachearte_buscar(s_url, NV_CACHE_ARTE_SMALL, &bytes, &n)) ok = 0;
    free(bytes); bytes = NULL;
    {
      unsigned char *sample = (unsigned char *)malloc(1024);
      NvCacheArteStats stats;
      if (!sample) ok = 0;
      else {
        memset(sample, 0x5a, 1024);
        sample[0] = 0xff; sample[1] = 0xd8; sample[2] = 0xff;
        sample[1023] = 0x37;
        cachearte_salvar(s_url, NV_CACHE_ARTE_SMALL, sample, 1024, 0);
        free(sample);
        for (int i = 0; i < 300; i++) {
          cachearte_estatisticas(&stats);
          if (stats.gravacoes > 0) break;
          pause_ms(10);
        }
        if (stats.gravacoes != 1) ok = 0;
      }
    }
    if (!cachearte_buscar(s_url, NV_CACHE_ARTE_SMALL, &bytes, &n)) ok = 0;
    if (n != 1024 || !bytes || bytes[0] != 0xff || bytes[1023] != 0x37) ok = 0;
    free(bytes); bytes = NULL;
    if (cachearte_buscar(s_url, NV_CACHE_ARTE_MEDIUM, &bytes, &n)) ok = 0;
    free(bytes);
    {
      unsigned char corrupt[700]; NvCacheArteStats stats;
      memset(corrupt, 0x41, sizeof corrupt);
      corrupt[0] = 0xff; corrupt[1] = 0xd8; corrupt[2] = 0xff;
      cachearte_salvar(s_url, NV_CACHE_ARTE_MEDIUM, corrupt, sizeof corrupt, 0);
      for (int i = 0; i < 300; i++) {
        cachearte_estatisticas(&stats);
        if (stats.gravacoes > 1) break;
        pause_ms(10);
      }
      if (stats.gravacoes != 2) ok = 0;
      if (!cachearte_buscar(s_url, NV_CACHE_ARTE_MEDIUM, &bytes, &n) || n != 700) ok = 0;
      free(bytes); bytes = NULL;
      /* The real texture decoder calls this after rejecting a body that passed
       * the inexpensive signature check. Verify that exact variant is removed. */
      cachearte_invalidar(s_url, NV_CACHE_ARTE_MEDIUM);
      if (cachearte_buscar(s_url, NV_CACHE_ARTE_MEDIUM, &bytes, &n)) ok = 0;
      free(bytes);
    }
  } else {
    if (!cachearte_buscar(s_url, NV_CACHE_ARTE_SMALL, &bytes, &n)) ok = 0;
    if (n != 1024 || !bytes || bytes[0] != 0xff || bytes[1023] != 0x37) ok = 0;
    free(bytes); bytes = NULL;
    if (cachearte_buscar(s_url, NV_CACHE_ARTE_MEDIUM, &bytes, &n)) ok = 0;
    free(bytes);
  }
  atomic_store(&s_result, ok ? 1 : -1);
  atomic_store(&s_done, 1);
  return NULL;
}

EMSCRIPTEN_KEEPALIVE void test_start(int stage) {
  pthread_t t;
  s_stage = stage;
  atomic_store(&s_done, 0); atomic_store(&s_result, 0);
  /* Exercise valid pin/clear calls before tex_cache_dir has initialized the
   * backend. The public bridge must initialize idempotently and preserve order. */
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, s_url,
                         NV_CACHE_ARTE_SMALL, 1, 1);
  cachearte_limpar_referencias_grupo(NV_CACHE_ARTE_GRUPO_HOME);
  cachearte_marcar_grupo(NV_CACHE_ARTE_GRUPO_HOME, s_url,
                         NV_CACHE_ARTE_SMALL, 1, 1);
  cachearte_iniciar();
  if (pthread_create(&t, NULL, worker, NULL) == 0) pthread_detach(t);
  else { atomic_store(&s_result, -2); atomic_store(&s_done, 1); }
}
EMSCRIPTEN_KEEPALIVE int test_done(void) { return atomic_load(&s_done); }
EMSCRIPTEN_KEEPALIVE int test_result(void) { return atomic_load(&s_result); }

static void *abort_worker(void *unused) {
  (void)unused;
  static const char *abort_url = "https://images.example/abort.jpg?rev=1";
  unsigned char sample[1024];
  NvCacheArteStats before, after;
  memset(sample, 0x5a, sizeof sample);
  sample[0] = 0xff; sample[1] = 0xd8; sample[2] = 0xff;
  cachearte_estatisticas(&before);
  cachearte_salvar(abort_url, NV_CACHE_ARTE_SMALL, sample, sizeof sample, 0);
  for (int i = 0; i < 300; i++) {
    cachearte_estatisticas(&after);
    if (after.falhas > before.falhas) break;
    pause_ms(10);
  }
  atomic_store(&s_abort_result,
      (int)(after.falhas - before.falhas) * 10 + (int)(after.gravacoes - before.gravacoes));
  atomic_store(&s_abort_done, 1);
  return NULL;
}
EMSCRIPTEN_KEEPALIVE void test_abort_write(void) {
  pthread_t t;
  if (pthread_create(&t, NULL, abort_worker, NULL) == 0) pthread_detach(t);
  else { atomic_store(&s_abort_result, -1); atomic_store(&s_abort_done, 1); }
}
EMSCRIPTEN_KEEPALIVE int test_abort_done(void) { return atomic_load(&s_abort_done); }
EMSCRIPTEN_KEEPALIVE int test_abort_result(void) { return atomic_load(&s_abort_result); }

static void *read_abort_worker(void *unused) {
  unsigned char *bytes = NULL;
  long n = 0;
  (void)unused;
  int miss = !cachearte_buscar(s_url, NV_CACHE_ARTE_SMALL, &bytes, &n);
  free(bytes);
  atomic_store(&s_read_abort_result, miss ? 1 : -1);
  atomic_store(&s_read_abort_done, 1);
  return NULL;
}
EMSCRIPTEN_KEEPALIVE void test_abort_read(void) {
  pthread_t t;
  atomic_store(&s_read_abort_done, 0); atomic_store(&s_read_abort_result, 0);
  if (pthread_create(&t, NULL, read_abort_worker, NULL) == 0) pthread_detach(t);
  else { atomic_store(&s_read_abort_result, -1); atomic_store(&s_read_abort_done, 1); }
}
EMSCRIPTEN_KEEPALIVE int test_read_abort_done(void) { return atomic_load(&s_read_abort_done); }
EMSCRIPTEN_KEEPALIVE int test_read_abort_result(void) { return atomic_load(&s_read_abort_result); }

static void *quota_worker(void *unused) {
  static const char *quota_url = "https://images.example/quota-retry.jpg?rev=1";
  unsigned char sample[1024], *readback = NULL;
  long n = 0;
  NvCacheArteStats before, after;
  (void)unused;
  memset(sample, 0x5a, sizeof sample);
  sample[0] = 0xff; sample[1] = 0xd8; sample[2] = 0xff;
  cachearte_estatisticas(&before);
  cachearte_salvar(quota_url, NV_CACHE_ARTE_SMALL, sample, sizeof sample, 0);
  for (int i = 0; i < 300; i++) {
    cachearte_estatisticas(&after);
    if (after.gravacoes > before.gravacoes || after.falhas > before.falhas) break;
    pause_ms(10);
  }
  int persisted = cachearte_buscar(quota_url, NV_CACHE_ARTE_SMALL, &readback, &n);
  atomic_store(&s_quota_result,
      (int)(after.gravacoes - before.gravacoes) * 100 +
      (int)(after.falhas - before.falhas) * 10 + (persisted && n == 1024 ? 1 : 0));
  free(readback);
  atomic_store(&s_quota_done, 1);
  return NULL;
}
EMSCRIPTEN_KEEPALIVE void test_quota_write(void) {
  pthread_t t;
  atomic_store(&s_quota_done, 0); atomic_store(&s_quota_result, 0);
  if (pthread_create(&t, NULL, quota_worker, NULL) == 0) pthread_detach(t);
  else { atomic_store(&s_quota_result, -1); atomic_store(&s_quota_done, 1); }
}
EMSCRIPTEN_KEEPALIVE int test_quota_done(void) { return atomic_load(&s_quota_done); }
EMSCRIPTEN_KEEPALIVE int test_quota_result(void) { return atomic_load(&s_quota_result); }
