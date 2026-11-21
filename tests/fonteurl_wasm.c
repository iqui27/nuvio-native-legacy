// F90 / PR573: contrato do receber_corpo de rede_pedir no WGT, nos DOIS
// motores de fetch do Emscripten — o padrao (-pthread, nv_http_fluxo via
// EM_ASYNC_JS) e o cooperativo (NV_UM_FIO, nv_http_fluxo_iniciar + espera que
// cede com fio1_ceder). O fetch e simulado pelo --pre-js
// tests/fonteurl_wasm.js, que tambem conta aborts do AbortController.
//
// O que este teste trava (reprova a6bce7d69 no modo NV_UM_FIO):
//   a) stream ABERTO + callback devolve 0: a primeira fatia tem de chegar ao
//      receber_corpo e o AbortController tem de disparar BEM ANTES do prazo —
//      acumular o corpo ate o EOF devolvia REDE_PRAZO sem chamar o callback.
//   b) EOF tardio (50 ms): a primeira fatia chega ANTES do fim do stream —
//      na versao quebrada o primeiro callback so vinha aos ~52 ms.
//   c) corpo de 32 MiB: a drenagem cede entre fatias, entao nenhum passo de
//      fio1_rodar passa do teto (a versao que drenava tudo num passo media
//      ~85 ms; aqui o teto e 16 ms).
// Alem disso preserva a matriz do F82: resposta de 250 ms com o escalonador
// girando, CORS (N), 404, prazo, corpo acima do teto e resposta sem leitor
// (E), e cancelamento/substituicao sem evento antigo nem staging residual.
#include "fio1.h"
#include "fonteurl.h"
#include "rede.h"
#include <assert.h>
#include <dirent.h>
#include <emscripten.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

uint64_t SDL_GetTicks64(void) { return 1; }
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}

static double run_frame(void) {
  double t = emscripten_get_now();
#ifdef NV_UM_FIO
  fio1_rodar(2);
#endif
  return emscripten_get_now() - t;
}

static int stage_count(void) {
  int count = 0;
  DIR *dir = opendir("/staging");
  assert(dir);
  for (struct dirent *e; (e = readdir(dir));) if (e->d_name[0] != '.') count++;
  closedir(dir);
  return count;
}

static void start_font(const char *path) {
  char url[128];
  snprintf(url, sizeof url, "https://fixture.invalid/%s", path);
  assert(fonteurl_iniciar(url, "/staging"));
}

// teto_ms > 0: nenhum passo de fio1_rodar pode passar desse teto (o passo e
// ~0 no modo pthread, onde nao ha fio1_rodar — a verificacao so morde no
// cooperativo).
static void font_case(const char *path, char expected, size_t bytes, double teto_ms) {
  char event[80] = {0};
  double slowest = 0, start = emscripten_get_now();
  int frames;
  start_font(path);
  for (frames = 0; frames < 800; frames++) {
    double ms = run_frame();
    if (ms > slowest) slowest = ms;
    if (fonteurl_resultado(event, sizeof event)) break;
    emscripten_sleep(2);
  }
  printf("font=%s event=%c frames=%d max_scheduler_ms=%.1f elapsed_ms=%.1f\n",
         path, event[0], frames, slowest, emscripten_get_now() - start);
  assert(event[0] == expected);
  if (bytes) {
    char file[128]; struct stat st;
    snprintf(file, sizeof file, "/staging/%s", event + 2);
    assert(!stat(file, &st) && (size_t)st.st_size == bytes);
    assert(!unlink(file));
  }
  assert(stage_count() == 0);
  if (teto_ms > 0) assert(slowest <= teto_ms);
}

typedef struct {
  atomic_int done;
  RedeResposta response;
  double started, first_callback, elapsed;
  size_t sent, max_slice;
  int callbacks, close_body, aborts;
} Transfer;

EM_JS(int, cancellations, (), { return globalThis.fixtureCancels || 0; });

static int reject_first(const void *bytes, size_t length, void *user) {
  Transfer *t = user;
  assert(((const unsigned char *)bytes)[0] == 7);
  ++t->callbacks;
  t->sent += length;
  if (length > t->max_slice) t->max_slice = length;
  t->first_callback = emscripten_get_now() - t->started;
  return 0;
}

static void *reject_body(void *user) {
  Transfer *t = user;
  RedePedido request = {.url = t->close_body ? "https://fixture.invalid/reject-close" : "https://fixture.invalid/reject",
                        .prazo_ms = 100, .receber_corpo = reject_first, .corpo_usuario = t};
  int before = cancellations();
  t->started = emscripten_get_now();
  assert(!rede_pedir(&request, &t->response));
  t->elapsed = emscripten_get_now() - t->started;
  t->aborts = cancellations() - before;
  atomic_store(&t->done, 1);
  return NULL;
}

EM_JS(int, inflight, (), { return globalThis.fixtureInflight || 0; });
EM_JS(int, requests, (), { return Module.nvFioReqs ? Object.keys(Module.nvFioReqs).length : 0; });

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(!mkdir("/staging", 0700));
  font_case("delayed", 'R', 16, 100.0);
  font_case("cors", 'N', 0, 0);
  font_case("http404", 'E', 0, 0);
  font_case("timeout", 'E', 0, 0);
  font_case("oversized", 'E', 0, 0);
  font_case("no-reader", 'E', 0, 0);
  font_case("large", 'R', 32u * 1024 * 1024, 16.0);

  for (int close_body = 0; close_body <= 1; ++close_body) {
    Transfer t = {.close_body = close_body}; pthread_t thread;
    assert(!pthread_create(&thread, NULL, reject_body, &t));
    for (int i = 0; i < 800 && !atomic_load(&t.done); i++) {
      run_frame();
      emscripten_sleep(2);
    }
    assert(atomic_load(&t.done));
    assert(!pthread_join(thread, NULL));
    printf("reject close=%d error=%d status=%d total=%zu callbacks=%d max_slice=%zu sent=%zu first_callback_ms=%.1f elapsed_ms=%.1f aborts=%d\n",
           close_body, t.response.erro, t.response.status, t.response.n_corpo, t.callbacks, t.max_slice,
           t.sent, t.first_callback, t.elapsed, t.aborts);
    assert(t.response.status == 200);
    assert(t.response.n_corpo == 20000);
    // (a)+(b): a primeira fatia chega ao callback antes do prazo e antes do
    // EOF de 50 ms; o retorno 0 aborta o fetch na hora — REDE_TRANSPORTE.
    assert(t.callbacks == 1 && t.max_slice == 16384 && t.sent == 16384);
    assert(t.response.erro == REDE_TRANSPORTE);
    assert(t.first_callback < 50.0 && t.elapsed < 80.0 && t.aborts == 1);
  }

  {
    char event[80];
    start_font("cancel");
#ifdef NV_UM_FIO
    // fixtureInflight e Module.nvFioReqs vivem no mesmo realm do fetch —
    // no modo pthread o fetch corre no worker e estes contadores ficam 0
    // aqui no fio principal; so o cooperativo os observa.
    for (int i = 0; i < 800 && !inflight(); i++) {
      run_frame();
      emscripten_sleep(2);
    }
    assert(inflight() == 1);
#endif
    fonteurl_cancelar();
    assert(!fonteurl_resultado(event, sizeof event));
    printf("after_cancel inflight=%d js_requests=%d staging=%d\n", inflight(), requests(), stage_count());
    font_case("replacement", 'R', 16, 0);
    for (int i = 0; i < 150; i++) {
      run_frame();
      assert(!fonteurl_resultado(event, sizeof event));
      emscripten_sleep(2);
    }
    printf("after_old_finishes inflight=%d js_requests=%d staging=%d\n", inflight(), requests(), stage_count());
#ifdef NV_UM_FIO
    assert(!inflight() && !requests());
#endif
    assert(!stage_count());
  }
  printf("=== OK fonteurl_wasm: contrato do receber_corpo cumprido\n");
  return 0;
}
