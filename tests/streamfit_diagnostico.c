/* Real diagnostic selection -> N01 streamed HTTP -> actual StreamFit history.
 * Dead-strip UI sections; no fake transport or whole-core compile. */
#include "../src/diagnostico.c"
#include "streamfit.h"
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

static char base[128];
static int port;
static uint64_t seq = 100;
// Diagnostic selection only needs the timer; no SDL/AppKit window/runtime.
Uint32 SDL_GetTicks(void) {
  struct timespec t; assert(!clock_gettime(CLOCK_MONOTONIC, &t));
  return (Uint32)((uint64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000);
}
static void novaRede(int conhecida) { redemarca_observar(++seq, conhecida); }
static StreamfitFoto foto(void) {
  StreamfitFoto f; streamfit_foto(&f, streamfit_agora_ms()); return f;
}
static void *cortar(void *modo) {
  usleep(300000);
  if ((long)modo == 1) novaRede(0);
  else atomic_store(&vz.cancelado, 1);
  return NULL;
}
static int medir(const char *path, int segundos, uint64_t cap, unsigned prazo, RedeVazao *r) {
  char url[512], final[4096]; int taxas[48];
  StreamfitDiagControle ctl = { .rede = redemarca_atual(), .prazo_ms = prazo, .cancelado = fitVazCancelado };
  snprintf(url, sizeof url, "%s%s", base, path);
  return streamfitdiag_medir(url, NULL, segundos, 0, cap, &ctl, taxas, 48, r, final, sizeof final);
}
int main(int argc, char **argv) {
  RedeVazao r; StreamfitFoto f; pthread_t thread;
  char final[4096], url[512]; RedePedido p = {0}; RedeResposta rr;
  assert(argc == 2); port = atoi(argv[1]);
  snprintf(base, sizeof base, "http://127.0.0.1:%d", port);
  assert(!redemarca_atual() && !foto().n);
  novaRede(1); uint64_t primeira = redemarca_atual();
  redemarca_observar(seq - 1, 0); assert(redemarca_atual() == primeira);
  assert(!medir("/fail", 7, 1 << 20, 0, &r) && r.status == 503 && !foto().n);
  assert(!medir("/truncated", 7, 1 << 20, 0, &r) && r.erro && !foto().n);
  assert(!medir("/media", 7, 4096, 0, &r) && !foto().n); // cap != completed window
  assert(!medir("/media", 7, 1 << 20, 300, &r) && r.erro && !foto().n);
  for (long mode = 0; mode < 2; mode++) {
    assert(!pthread_create(&thread, NULL, cortar, (void *)mode));
    assert(!medir("/stall", 7, 1 << 20, 0, &r));
    pthread_join(thread, NULL);
    assert(r.cancelado && !foto().n);
    atomic_store(&vz.cancelado, 0); novaRede(1);
  }
  // Five genuine intervals of HTML, mislabeled HTML and unknown MIME still
  // cannot become a bandwidth observation. No source URL is guessed valid.
  assert(!medir("/html", 6, 1 << 20, 0, &r) && !foto().n);
  assert(!medir("/fake-video", 6, 1 << 20, 0, &r) && !foto().n);
  // #203: a generic octet-stream body is MEASURED (debrid CDNs serve files so),
  // but it never becomes a stream-fit observation: that needs a video/ MIME.
  assert(medir("/unknown", 6, 1 << 20, 0, &r) > 0 && !foto().n);
  // Production diagnostic branch: do not pre-resolve with the legacy HEAD;
  // real final response supplies MIME and authority. Caller header is stripped.
  snprintf(url, sizeof url, "%s/redirect?synthetic=hidden", base);
  VazCand c = { .url = url, .cab = "X-Private-Key: synthetic" };
  VazFonte mf; VazResultado cat; int motivo; char chave[96], pub[96];
  vz.rede = redemarca_atual();
  assert(medirNucleo(&c, 7, NULL, 0, &mf, &r, chave, pub, sizeof pub, &cat, &motivo) == MN_OK);
  // #203: the private header is stripped, but Range reaches the CDN hop (206).
  assert(mf.n >= 5 && mf.n < 9 && cat == VR_OK && r.status == 206);
  int zeros = 0;
  for (int i = 0; i < mf.n; i++) if (!mf.kbps[i]) zeros++;
  assert(zeros > 0); // measured idle wire interval, included in the budget
  f = foto(); assert(f.n == 1 && strstr(f.hosts[0].host, "localhost:"));
  assert(!strstr(f.hosts[0].host, "hidden") && !strstr(f.hosts[0].host, "synthetic"));
  assert(strstr(chave, "localhost:") && strstr(pub, "localhost:"));
  snprintf(final, sizeof final, "http://localhost:%d/movie", port);
  assert(streamfit_classificar(&f, final, 100000, 100, NULL) != SF_DESCONHECIDA);
  assert(streamfit_classificar(&f, url, 100000, 100, NULL) == SF_DESCONHECIDA); // resolver != final host
  uint64_t velha = redemarca_atual(); novaRede(0); assert(!foto().n && !redemarca_atual());
  int taxas[5] = {2000,2000,2000,2000,2000};
  assert(!streamfit_diagnostico(velha, final, taxas, 5, streamfit_agora_ms()));
  novaRede(1); assert(!foto().n); // even returning to the old route needs retest
  // N01 discard mode never buffers gigabytes and exposes deliberate stop.
  snprintf(url, sizeof url, "%s/media", base);
  p.url = url; p.janela_corpo_ms = 1000; p.max_descartado = 1 << 20;
  assert(rede_pedir(&p, &rr) && rr.fim_janela && !rr.fim_teto);
  assert(!rr.corpo && !rr.n_corpo && rr.n_prefixo == 512 && rr.bytes_fio < (1 << 20));
  rede_resposta_limpar(&rr);
  // An invalid discard contract must not issue a request.
  p.max_descartado = 0; assert(!rede_pedir(&p, &rr) && rr.erro == REDE_ENTRADA);
  rede_resposta_limpar(&rr);
  puts("streamfit_diagnostico: PASS (real media/final host, discard, MIME/HTML, caps, truncation, deadline, cancel, network change)");
}
