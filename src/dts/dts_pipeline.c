#define _POSIX_C_SOURCE 200809L
#include "dts_pipeline.h"
#include "../webosver.h"
#include "adapter/adapter.h"
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <pthread.h>
struct DtsPipeline { void *library, *native; const DtsAdapter *api; };
static void *open_adapter(int major, const DtsAdapter **api) {
  char path[PATH_MAX], executable[PATH_MAX];
  const char *override = getenv("NUVIO_DTS_ADAPTER_DIR");
  void *library;
  const DtsAdapter *(*entry)(void);
  ssize_t n;
  *api = NULL;
  if (major == 0) {
    void *auto_lib = open_adapter(4, api);
    return auto_lib ? auto_lib : open_adapter(3, api);
  }
  if (major < 3) return NULL;
  if (override && *override) {
    if (snprintf(path, sizeof path, "%s/dts-starfish-webos%d.so", override,
                 major == 3 ? 3 : 4) >= (int)sizeof path) return NULL;
  } else {
    n = readlink("/proc/self/exe", executable, sizeof executable - 1);
    if (n <= 0) return NULL;
    executable[n] = 0;
    char *slash = strrchr(executable, '/');
    if (!slash) return NULL;
    *slash = 0;
    if (snprintf(path, sizeof path, "%s/lib/dts-starfish-webos%d.so", executable,
                 major == 3 ? 3 : 4) >= (int)sizeof path) return NULL;
  }
  library = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (!library) {
    fprintf(stderr, "[dts] adapter load failed: %s: %s\n", path, dlerror());
    return NULL;
  }
  *(void **)(&entry) = dlsym(library, "nuvio_dts_adapter_v2");
  if (!entry) goto fail;
  *api = entry();
  if (!*api || (*api)->abi != DTS_ADAPTER_ABI || (*api)->size != sizeof(DtsAdapter) ||
      !(*api)->probe || !(*api)->create || !(*api)->destroy || !(*api)->load ||
      !(*api)->feed || !(*api)->play || !(*api)->pause || !(*api)->flush ||
      !(*api)->eos || !(*api)->media_id || !(*api)->error || !(*api)->probe()) goto fail;
  return library;
fail:
  fprintf(stderr, "[dts] adapter unavailable: %s (ABI or firmware probe failed)\n", path);
  *api = NULL; dlclose(library); return NULL;
}
/* O SIM vale para o processo inteiro: o firmware nao muda com o app aberto e
 * nenhum ajuste muda o que a TV carrega. Antes cada video sondava de novo, e
 * no webOS 3 isso era um dlopen do webos4 que sempre falha (GLIBCXX_3.4.21)
 * mais o do webos3 e o da libplayerAPIs.
 * O NAO so fica guardado no webOS 3 ou anterior, onde nao ha conversao a
 * perder. No webOS 4+ (e com versao desconhecida) um nao pode ser passageiro
 * e guarda-lo desligaria o DTS ate reabrir o app: la cada video sonda de novo,
 * como sempre foi.
 * Uma posicao por pedido (0 = automatico, 3, 4+); -1 = sem resposta guardada.
 * A trava cobre dois fios perguntando juntos: o segundo espera o primeiro. */
static pthread_mutex_t sondaTrava = PTHREAD_MUTEX_INITIALIZER;
static int sondaResposta[3] = {-1, -1, -1};
static int sondaPosicao(int major) { return major == 0 ? 0 : major == 3 ? 1 : 2; }
/* webOS da TV: a fonte unica do webosver.c (nyx, depois starfish-release).
 * 0 = nao deu para saber. */
static int sondaWebos(void) { return nv_webos_major(); }
static int sondar(int major) {
  const DtsAdapter *api;
  void *lib = open_adapter(major, &api);
  if (!lib) return 0;
  printf("[dts] firmware adapter ready\n"); fflush(stdout);
  dlclose(lib); return 1;
}
int dts_pipeline_available(int major) {
  int i, r;
  if (major != 0 && major < 3) return 0;
  i = sondaPosicao(major);
  pthread_mutex_lock(&sondaTrava);
  r = sondaResposta[i];
  if (r < 0) {
    int webos = sondaWebos();
    r = sondar(major);
    if (r || (webos > 0 && webos < 4)) sondaResposta[i] = r;
  }
  pthread_mutex_unlock(&sondaTrava);
  return r;
}
void dts_pipeline_available_esquecer(void) {
  pthread_mutex_lock(&sondaTrava);
  for (int i = 0; i < 3; i++) sondaResposta[i] = -1;
  pthread_mutex_unlock(&sondaTrava);
}
DtsPipeline *dts_pipeline_create(const char *app, const char *window, int major,
                                void (*event)(void *, const char *), void *user) {
  DtsPipeline *p = calloc(1, sizeof *p);
  if (!p) return NULL;
  p->library = open_adapter(major, &p->api);
  if (!p->library) { free(p); return NULL; }
  p->native = p->api->create(app, window, event, user);
  if (!p->native) { dlclose(p->library); free(p); return NULL; }
  return p;
}
void dts_pipeline_destroy(DtsPipeline *p) {
  if (!p) return;
  p->api->destroy(p->native); dlclose(p->library); free(p);
}
int dts_pipeline_load(DtsPipeline *p, const DtsMediaInfo *m, double t) { return p && m ? p->api->load(p->native,m,t) : 0; }
int dts_pipeline_feed(DtsPipeline *p, const DtsFrame *f) { return p && f ? p->api->feed(p->native,f) : -1; }
int dts_pipeline_play(DtsPipeline *p) { return p ? p->api->play(p->native) : 0; }
int dts_pipeline_pause(DtsPipeline *p) { return p ? p->api->pause(p->native) : 0; }
int dts_pipeline_flush(DtsPipeline *p, double t) { return p ? p->api->flush(p->native,t) : 0; }
int dts_pipeline_eos(DtsPipeline *p) { return p ? p->api->eos(p->native) : 0; }
const char *dts_pipeline_media_id(DtsPipeline *p) { return p ? p->api->media_id(p->native) : ""; }
const char *dts_pipeline_error(DtsPipeline *p) { return p ? p->api->error(p->native) : "native DTS adapter unavailable or ABI mismatch"; }

int dts_pipeline_volume(DtsPipeline *p, int pct) {
  return p && p->api->volume ? p->api->volume(p->native, pct) : 0;
}
