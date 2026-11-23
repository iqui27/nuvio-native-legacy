// Directed tests for the .tpk 6+ / webOS seek-thumbnail provider (src/miniatura.c,
// PR 680 stage B). The whole TU is included below and its statics are driven
// directly; every outside dependency is faked — engine (mini_motor_*), source
// reads (rede_baixar_trecho64_final), player (video_*), GL uploads, SDL ticks
// and the /proc memory reads (NV_MINIATURA_MEM_KB / NV_MINIATURA_RAM_MB macros).
//
//   A. THE GATE — the same rule as the TV guide (memoria.h): forced Baixa, or
//      Auto with unknown / <= 2048 MB RAM, is "pouca" and keeps the provider
//      off; a package without the FFmpeg tools hides it too.
//   B. THE GRID — 5 s for <=1080p <=90 min, or >1080p <70 min with >=2 GB;
//      10 s otherwise, unknown RAM included, and every MPEG-TS/M2TS.
//   C. ORDER — scrub point first, then around the playhead, then the film.
//   D. MEMORY — MemAvailable must cover the 150 MB floor + the next frame's
//      cost (measured on the device x 1.25); while short, the worker pauses
//      and releases the decoder; the JPEG set stays bounded by the slots.
//   E. DISPLAY — the nearest made tile (<= 5 min) shows dimmed with ITS own
//      time; uploads are an SLOTS-slot LRU.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

// test-owned probes (NV_MINIATURA_MEM_KB / NV_MINIATURA_RAM_MB expand to these)
static long memFakeKB = 900L * 1024;
static long ramFakeMB = 4096;
static unsigned tickFake = 100000;
#define NV_MINIATURA_MEM_KB __atomic_load_n(&memFakeKB, __ATOMIC_ACQUIRE)
#define NV_MINIATURA_RAM_MB __atomic_load_n(&ramFakeMB, __ATOMIC_ACQUIRE)
static void memFake(long kb) { __atomic_store_n(&memFakeKB, kb, __ATOMIC_RELEASE); }

#include "../src/miniatura.c"

// ----- fake state ------------------------------------------------------------
static MiniInfo fakeInfo;
static int ferramentasStub = 1;
static int motorQuadros, motorSoltadas, motorFechado, motorAberto;
static size_t motorMemoriaBytes = 40L << 20;   // the "next frame" estimate
static long quadroCustoKB;                     // MemAvailable each fake tile eats
static int motorFalhar;
static int motorDecAberto;                     // decoder open? (mini_motor_decoder_aberto)
static int fakePronto = 1, fakeTocando = 1, fakeDts, fakeLargura;
static unsigned fakeBufferando;
static double fakePos;
static int redePedidos, redeFalhar;
static int texUploads, texGeradas, texDeletadas;

// ----- stub implementations (linked instead of the real ones) -----------------
int perftv_enabled, perftv_uploads, perftv_blur, perftv_glass;
Uint32 SDL_GetTicks(void) { return tickFake; }
SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) { (void)mem; (void)size; return (SDL_RWops *)1; }
SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc) { (void)src; (void)freesrc; return NULL; }
SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 fmt, Uint32 flags) { (void)src; (void)fmt; (void)flags; return NULL; }
int SDL_LockSurface(SDL_Surface *s) { (void)s; return 0; }
void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }
void SDL_FreeSurface(SDL_Surface *s) { if (s) { free(s->format); free(s->pixels); free(s); } }
#undef glTexImage2D
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const GLvoid *p) {
  (void)t; (void)l; (void)i; (void)w; (void)h; (void)b; (void)f; (void)ty; (void)p; texUploads++;
}
void glGenTextures(GLsizei n, GLuint *t) { for (int i = 0; i < n; i++) t[i] = ++texGeradas; }
void glDeleteTextures(GLsizei n, const GLuint *t) { for (int i = 0; i < n; i++) if (t[i]) texDeletadas++; }
void glBindTexture(GLenum t, GLuint x) { (void)t; (void)x; }
void glPixelStorei(GLenum p, GLint v) { (void)p; (void)v; }
void glTexParameteri(GLenum t, GLenum p, GLint v) { (void)t; (void)p; (void)v; }
void gfx_tex_esquecer(GLuint t) { (void)t; }
SDL_Surface *jpeg_rapido_carregar_mem(const unsigned char *dados, size_t n, int largMax,
                                      int *larguraOriginal, int *alturaOriginal) {
  (void)dados; (void)largMax;
  if (n < 10) return NULL;
  SDL_Surface *s = calloc(1, sizeof *s);
  SDL_PixelFormat *f = calloc(1, sizeof *f);
  if (!s || !f) { free(s); free(f); return NULL; }
  f->format = SDL_PIXELFORMAT_ABGR8888;
  s->format = f; s->w = 320; s->h = 180; s->pitch = 320 * 4; s->flags = 0;
  s->pixels = malloc((size_t)s->pitch * s->h);
  memset(s->pixels, 0x5a, (size_t)s->pitch * s->h);
  if (larguraOriginal) *larguraOriginal = 320;
  if (alturaOriginal) *alturaOriginal = 180;
  return s;
}
char *rede_baixar_trecho64_final(const char *url, const char *headers,
                                 int64_t start, int64_t end, long *size,
                                 int64_t *total, int *status,
                                 volatile int *cancelled, char *final, int *cross) {
  (void)headers; (void)cancelled; (void)url;
  redePedidos++;
  if (redeFalhar) { *status = 0; *size = 0; return NULL; }
  int64_t tam = end - start + 1;
  if (tam > 65536) tam = 65536;
  char *b = malloc((size_t)tam);
  if (!b) { *status = 0; *size = 0; return NULL; }
  memset(b, 0x33, (size_t)tam);
  *size = (long)tam; *total = 200LL << 20; *status = 206;
  if (final) final[0] = 0;
  if (cross) *cross = 0;
  return b;
}
int video_pronto(void) { return fakePronto; }
int video_tocando(void) { return fakeTocando; }
unsigned video_bufferando_ms(void) { return __atomic_load_n(&fakeBufferando, __ATOMIC_RELAXED); }
double video_pos(void) { return fakePos; }
int video_dts_estado(void) { return fakeDts; }
int ajustes_miniatura_largura(void) { return fakeLargura; }

// The video/ajustes state the player publishes on every pedir (SeekrFonte).
// With the snapshot (NV_MINIATURA_SNAP) this writes the copy the worker
// reads; on the old revision the worker polled the stubs above from its own
// thread — the same globals make both paths say the same thing.
static void publicar(int pronto, int tocando, unsigned bufferandoMs, int dts, long playheadMs, int largura) {
  fakePronto = pronto; fakeTocando = tocando; fakeDts = dts; fakeLargura = largura;
  __atomic_store_n(&fakeBufferando, bufferandoMs, __ATOMIC_RELAXED);
  fakePos = playheadMs / 1000.0;
#ifdef NV_MINIATURA_SNAP
  pthread_mutex_lock(&mu);
  videoEst.pronto = pronto; videoEst.tocando = tocando;
  videoEst.bufferandoMs = bufferandoMs; videoEst.dts = dts;
  videoEst.playheadMs = playheadMs; videoEst.largura = largura;
  pthread_mutex_unlock(&mu);
#endif
}

MiniMotor *mini_motor_abrir(MiniLer ler, void *usuario, int64_t tamanho,
                            char *erro, size_t nerro) {
  (void)usuario; (void)tamanho;
  if (motorFalhar > 1) { snprintf(erro, nerro, "fake: container refused"); return NULL; }
  motorAberto++;
  motorDecAberto = 0;                        // a fresh motor has no decoder yet
  return (MiniMotor *)ler;   // the pointer only needs to be non-NULL and stable
}
const MiniInfo *mini_motor_info(const MiniMotor *m) { (void)m; return &fakeInfo; }
long mini_motor_chave(MiniMotor *m, long alvoMs, long tolMs) { (void)m; (void)tolMs; return alvoMs; }
size_t mini_motor_memoria(const MiniMotor *m, int threads) { (void)m; (void)threads; return motorMemoriaBytes; }
int mini_motor_quadro(MiniMotor *m, long alvoMs, long tolMs, int threads,
                      volatile int *cancelar, unsigned char **jpg, int *njpg,
                      MiniQuadro *q, char *erro, size_t nerro) {
  (void)m; (void)tolMs; (void)threads; (void)cancelar;
  if (motorFalhar) { snprintf(erro, nerro, "fake: decode failed"); return -1; }
  motorDecAberto = 1;                       // the real motor opens it in here
  __atomic_fetch_add(&motorQuadros, 1, __ATOMIC_ACQ_REL);
  __atomic_fetch_sub(&memFakeKB, quadroCustoKB, __ATOMIC_ACQ_REL);   // what the frame really cost
  if (NV_MINIATURA_MEM_KB < 0) memFake(0);
  *jpg = malloc(800); *njpg = 800;
  memset(*jpg, 0x4a, 800);
  *q = (MiniQuadro){ .ptsMs = alvoMs, .decodMs = 12, .totalMs = 60,
                     .bytesLidos = 60000, .bytesPacote = 50000, .threads = 1, .pedidos = 2 };
  return 0;
}
void mini_motor_soltar_decoder(MiniMotor *m) { (void)m; motorDecAberto = 0; __atomic_fetch_add(&motorSoltadas, 1, __ATOMIC_ACQ_REL); }
int mini_motor_decoder_aberto(const MiniMotor *m) { (void)m; return motorDecAberto; }
void mini_motor_config_padrao(MiniConfig *c) { memset(c, 0, sizeof *c); c->lerBlocos = 4; c->tsBlocos = 7; c->prefetch = 1; }
void mini_motor_config(MiniMotor *m, const MiniConfig *c) { (void)m; (void)c; }
void mini_motor_proximo(MiniMotor *m, long alvoMs, long tolMs) { (void)m; (void)alvoMs; (void)tolMs; }
long long mini_motor_lidos(const MiniMotor *m) { (void)m; return (long long)redePedidos * 65536; }
void mini_motor_fechar(MiniMotor *m) { (void)m; motorFechado++; }
int mini_motor_tem_ferramentas(void) { return ferramentasStub; }
void mini_impressao(uint64_t tamanho, const unsigned char *cabeca, size_t ncabeca,
                    const unsigned char *cauda, size_t ncauda, char saida[65]) {
  (void)tamanho; (void)cabeca; (void)ncabeca; (void)cauda; (void)ncauda;
  memset(saida, 'a', 64); saida[64] = 0;
}

// ---------------------------------------------------------------------------
static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); falhas++; } } while (0)

static Job *novoJob(int n, long dur) {
  Job *j = calloc(1, sizeof *j);
  j->tiles = calloc((size_t)n, sizeof *j->tiles);
  j->falhas = calloc((size_t)n, 1);
  j->n = n; j->dur = dur; j->refs = 2; j->vivo = 1; j->g = ++geracao;
  return j;
}
static void marcarFeito(Job *j, int k) {
  j->tiles[k].jpg = malloc(800); j->tiles[k].n = 800; j->tiles[k].pts = k * BASE_MS; j->feitos++;
}
static Trabalho *novoTrabalho(Job *j) {
  Trabalho *w = calloc(1, sizeof *w);
  w->j = j;
  snprintf(w->url, sizeof w->url, "%s", "https://fonte.invalid/filme.mkv");
  return w;
}
static void esperar(int (*cond)(void *), void *u, int msMax) {
  for (int i = 0; i * 5 < msMax; i++) { if (cond(u)) return; usleep(5000); }
}
// The worker owns these fields under mu; the test reads them the same way.
static int jGet(Job *j, int oque) {
  pthread_mutex_lock(&mu);
  int v = oque == 0 ? j->completo : oque == 1 ? j->vivo : j->feitos;
  pthread_mutex_unlock(&mu);
  return v;
}
static int completoF(void *u) { Job *j = u; return jGet(j, 0) || !jGet(j, 1); }

int main(void) {
  snprintf(fakeInfo.formato, sizeof fakeInfo.formato, "%s", "matroska");
  snprintf(fakeInfo.codec, sizeof fakeInfo.codec, "%s", "h264");
  fakeInfo.largura = 1920; fakeInfo.altura = 1080; fakeInfo.bits = 8; fakeInfo.chaves = 120;

  // ---- A. the gate: the SAME rule as the TV guide ----------------------------
  CHECK(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 2048), "auto, 2048 MB -> pouca");
  CHECK(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 1024), "auto, 1024 MB -> pouca");
  CHECK(!epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 2049), "auto, 2049 MB -> cheia");
  CHECK(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 0), "auto, RAM desconhecida -> pouca");
  CHECK(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, -1), "auto, RAM negativa -> pouca");
  CHECK(epg_memoria_eh_baixa(EPG_MEMORIA_BAIXA, 65536), "baixa forcada -> pouca");
  CHECK(!epg_memoria_eh_baixa(EPG_MEMORIA_COMPLETA, 256), "completa forcada -> cheia");
  CHECK(epg_memoria_eh_baixa(99, 1024), "modo fora da faixa -> auto -> pouca");
  CHECK(!epg_memoria_eh_baixa(99, 8192), "modo fora da faixa -> auto -> cheia");

  ferramentasStub = 1;
  CHECK(!miniatura_tv_capaz(EPG_MEMORIA_AUTO, 2048), "capaz: auto 2048 -> escondido");
  CHECK(!miniatura_tv_capaz(EPG_MEMORIA_AUTO, 0), "capaz: auto desconhecida -> escondido");
  CHECK(!miniatura_tv_capaz(EPG_MEMORIA_BAIXA, 8192), "capaz: baixa forcada -> escondido");
  CHECK(miniatura_tv_capaz(EPG_MEMORIA_AUTO, 4096), "capaz: auto 4096 -> oferecido");
  CHECK(miniatura_tv_capaz(EPG_MEMORIA_COMPLETA, 512), "capaz: completa forcada -> oferecido");
  CHECK(miniatura_tv_provedor() == &PROVEDOR, "provedor existe com as ferramentas");
  ferramentasStub = 0;
  CHECK(miniatura_tv_provedor() == NULL, "pacote sem FFmpeg de miniatura -> sem provedor");
  CHECK(!miniatura_tv_capaz(EPG_MEMORIA_AUTO, 8192), "capaz: ferramentas faltando -> escondido");
  ferramentasStub = 1;

  // ---- B. the adaptive grid --------------------------------------------------
  {
    MiniInfo i = fakeInfo; const char *pq = NULL;
    CHECK(gradeDe(&i, 60L * 60000, 1024, &pq) == 1, "1080p 60 min -> grade 5 s");
    CHECK(gradeDe(&i, 90L * 60000, 1024, &pq) == 1, "1080p 90 min -> grade 5 s");
    CHECK(gradeDe(&i, 91L * 60000, 8192, &pq) == DEZ, "1080p 91 min -> grade 10 s (%s)", pq);
    i.largura = 2048; i.altura = 1080;
    CHECK(gradeDe(&i, 80L * 60000, 1024, &pq) == 1, "2048 colunas ainda conta como <=1080p");
    i.largura = 3840; i.altura = 2160;
    CHECK(gradeDe(&i, 30L * 60000, 2048, &pq) == 1, "4K <70 min com 2 GB -> grade 5 s");
    CHECK(gradeDe(&i, 30L * 60000, 2047, &pq) == DEZ, "4K <70 min com <2 GB -> grade 10 s");
    CHECK(gradeDe(&i, 30L * 60000, -1, &pq) == DEZ, "4K com RAM desconhecida -> grade 10 s (%s)", pq);
    CHECK(gradeDe(&i, 70L * 60000, 8192, &pq) == DEZ, "4K >=70 min -> grade 10 s");
    snprintf(i.formato, sizeof i.formato, "%s", "mpegts");
    CHECK(gradeDe(&i, 30L * 60000, 8192, &pq) == DEZ, "M2TS 4K curto -> grade 10 s (%s)", pq);
    i.largura = 720; i.altura = 576;
    CHECK(gradeDe(&i, 30L * 60000, 8192, &pq) == DEZ, "M2TS 480p curto -> grade 10 s (%s)", pq);
  }

  // ---- C. measured cost -------------------------------------------------------
  {
    MiniInfo i4k = { .largura = 3840, .altura = 2160 };
    MiniInfo ihd = { .largura = 1920, .altura = 1080 };
    long med[3] = { 20000, 40000, 30000 };
    long baixa1[1] = { 1000 };
    CHECK(custoEstimadoKB(&i4k, NULL, 0, 30000) == CUSTO_4K_KB, "4K sem medida -> chute 60 MB");
    CHECK(custoEstimadoKB(&ihd, NULL, 0, 30000) == 30000, "1080p: o motor acima do chute vence");
    CHECK(custoEstimadoKB(&ihd, NULL, 0, 10000) == 15360, "1080p: chute escala com a imagem");
    CHECK(custoEstimadoKB(&i4k, med, 3, 90000) == 50000, "medidas: pior x 1.25 vence o motor");
    CHECK(custoEstimadoKB(&i4k, baixa1, 1, 90000) == MEDIDA_MIN_KB, "piso de 20 MB para medidas minimas");
  }

  // ---- D. ordering of work ----------------------------------------------------
  {
    Job *j = novoJob(100, 100 * BASE_MS);   // 8:20, 100 slots
    int classe = 0, k;
    j->passo = DEZ;
    alvoScrub = 40 * BASE_MS; alvoTick = tickFake;
    k = proximo(j, 50 * BASE_MS, &classe);
    CHECK(k == 40 && classe == 1, "o ponto do scrub e o primeiro (k=%d, etapa %d)", k, classe);
    marcarFeito(j, 40);
    CHECK(proximo(j, 50 * BASE_MS, &classe) == 42 && classe == 1, "janela do scrub para a frente primeiro");
    for (int s = 34; s <= 46; s += 2) if (!j->tiles[s].jpg) marcarFeito(j, s);
    CHECK(proximo(j, 50 * BASE_MS, &classe) == 50 && classe == 2, "depois +-1 min do playhead");
    marcarFeito(j, 50);
    for (int s = 48; s <= 62; s += 2) if (!j->tiles[s].jpg) marcarFeito(j, s);
    k = proximo(j, 50 * BASE_MS, &classe);
    CHECK((k == 0 || k == 60) && classe == 3, "depois um a cada 5 min (k=%d)", k);
    if (!j->tiles[0].jpg) marcarFeito(j, 0);
    int visto[101] = {0};
    int total = j->feitos, sobra = 0;
    for (int s = 0; s < 100; s++) if (!j->tiles[s].jpg && s % DEZ == 0) sobra++;
    for (int t = 0; t < sobra; t++) {
      k = proximo(j, 50 * BASE_MS, &classe);
      assert(k >= 0);
      CHECK(k % DEZ == 0 && !visto[k], "slot %d fora da grade ou repetido", k);
      visto[k] = 1;
      marcarFeito(j, k);
    }
    CHECK(proximo(j, 50 * BASE_MS, &classe) < 0, "grade cheia -> nada a fazer");
    CHECK(total + sobra == 50, "100 slots na grade 10 s = 50 tiles (veio %d)", total + sobra);
    // Odd slots are never made on the 10 s grid (the same tile serves two times).
    for (int s = 1; s < 100; s += 2) CHECK(!j->tiles[s].jpg, "slot impar %d virou tile na grade 10 s", s);
    soltarJob(j); soltarJob(j);
  }
  {
    // 5 s grid: the whole scrub window every 5 s first, then +-1 min every 10 s.
    Job *j = novoJob(100, 100 * BASE_MS);
    int classe = 0;
    j->passo = 1;
    alvoScrub = 40 * BASE_MS; alvoTick = tickFake;
    CHECK(proximo(j, 50 * BASE_MS, &classe) == 40 && classe == 1, "5 s: scrub primeiro");
    for (int s = 34; s <= 46; s++) marcarFeito(j, s);
    int k = proximo(j, 50 * BASE_MS, &classe);
    CHECK(k == 50 && classe == 2, "5 s: depois o playhead a cada 10 s (k=%d etapa %d)", k, classe);
    soltarJob(j); soltarJob(j);
  }

  alvoScrub = -1;   // a quiet playhead for the worker tests
  snprintf(fakeInfo.formato, sizeof fakeInfo.formato, "%s", "mpegts");   // always the 10 s grid

  // ---- E. the worker: memory floor, bounded set, completion -------------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);    // 2:00 -> 12 tiles on the 10 s grid
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    quadroCustoKB = 0;
    publicar(1, 1, 0, 0, 0, 0);            // ready and playing, playhead at 0
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0), "worker termina a grade inteira");
    CHECK(jGet(j, 2) == 12, "24 slots na grade 10 s = 12 tiles (feitos=%d)", jGet(j, 2));
    CHECK(motorQuadros == 12, "um decode por tile (quadros=%d)", motorQuadros);
    for (int s = 0; s < 24; s++) {
      if (s % DEZ == 0) CHECK(j->tiles[s].jpg && j->tiles[s].n == 800, "tile do slot %d", s);
      else CHECK(!j->tiles[s].jpg, "slot impar %d sem tile", s);
    }
    long bytes = 0;
    for (int s = 0; s < 24; s++) bytes += j->tiles[s].n;
    CHECK(bytes == 12 * 800, "cache de JPEGs limitado aos slots (%ld bytes)", bytes);
    CHECK(motorFechado == 1, "motor fechado no fim");
    CHECK(miniatura_tv_status_agora() == 1, "marco 'iniciadas'");
    tickFake += 5000;
    CHECK(miniatura_tv_status_agora() == 2, "marco '50%%'");
    tickFake += 5000;
    CHECK(miniatura_tv_status_agora() == 3, "marco 'geradas' depois da grade cheia");
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- F. the memory floor pauses the worker ----------------------------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    int antes = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE), soltaram = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE);
    memFake(100 * 1024);                 // 100 MB free: below the 150 MB floor
    publicar(1, 1, 0, 0, 0, 0);
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    usleep(1200000);                        // > the opening settle + a pause cycle
    CHECK(__atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) == antes, "memoria abaixo do piso: nenhum quadro decodificado");
    CHECK(__atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) > soltaram, "decoder solto enquanto pausado");
    CHECK(!jGet(j, 0) && jGet(j, 1), "pausado, nao morto");
    // Free memory returns: the worker resumes and finishes the set.
    memFake(900L * 1024);
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0) && jGet(j, 2) == 12, "memoria livre: a grade termina (feitos=%d)", jGet(j, 2));
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- G. the measured cost tightens the gate ---------------------------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    quadroCustoKB = 40000;                  // ~39 MB per frame -> measured x1.25 = ~48.8 MB
    __atomic_store_n(&motorQuadros, 0, __ATOMIC_RELEASE);
    memFake(1000L * 1024);
    publicar(1, 1, 0, 0, 0, 0);
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    for (int i = 0; i < 600 && !jGet(j, 2); i++) usleep(5000);
    CHECK(jGet(j, 2) >= 1, "primeiro tile feito com o chute");
    // After the first tile the estimate becomes ~50 MB; leave ~170 MB free:
    // above the 150 MB floor + old guess, BELOW floor + measured cost.
    memFake(170L * 1024);
    usleep(400000);
    int feitosAgora = jGet(j, 2);
    usleep(400000);
    CHECK(jGet(j, 2) == feitosAgora && feitosAgora < 12, "custo medido aperta o portao: pausado (feitos=%d)", feitosAgora);
    memFake(900L * 1024);
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0), "com memoria de volta a grade termina");
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- J. paused by the control: no decode, decoder released -------------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    quadroCustoKB = 0;
    memFake(900L * 1024);
    int q0 = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE);
    int s0 = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE);
    publicar(1, 0, 0, 0, 0, 0);             // ready BUT paused by the control
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    usleep(1200000);                         // the heartbeat stays fresh the whole time
    CHECK(__atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) == q0,
          "pronto e pausado: nenhum quadro decodificado (quadros=%d)",
          __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) - q0);
    CHECK(__atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) > s0, "pausado: o decoder e solto");
    CHECK(!jGet(j, 0) && jGet(j, 1), "pausado, nao morto");
    publicar(1, 1, 0, 0, 0, 0);             // playing again: the grid finishes
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0) && jGet(j, 2) == 12, "retomada: a grade termina (feitos=%d)", jGet(j, 2));
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- K. decoder released: the gate charges the cold estimate -----------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    quadroCustoKB = 1024;                    // each tile eats 1 MB of MemAvailable
    memFake(900L * 1024);
    publicar(1, 1, 0, 0, 0, 0);
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    for (int i = 0; i < 600 && !jGet(j, 2); i++) usleep(5000);
    CHECK(jGet(j, 2) >= 1, "primeiro tile feito com a estimativa fria");
    // Buffering releases the decoder; the measured delta is ~1 MB x1.25 = 20 MB.
    int s1n = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE);
    publicar(1, 1, 2000, 0, 0, 0);
    for (int i = 0; i < 400 && __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) <= s1n; i++) usleep(5000);
    CHECK(__atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) > s1n, "buffering: decoder solto");
    int q1 = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE);   // soltar vem depois do quadro no fio
    // Resume with 175 MB free: above floor + measured 20 MB (170), below floor
    // + cold estimate 40 MB (190) — the frame that reopens the decoder must wait.
    memFake(175L * 1024);
    publicar(1, 1, 0, 0, 0, 0);
    usleep(1200000);
    CHECK(__atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) == q1,
          "reabrir o decoder cobra a estimativa fria: 175 MB nao bastam (quadros=%d)",
          __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) - q1);
    memFake(900L * 1024);
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0), "com memoria de volta a grade termina");
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- L. unreadable MemAvailable blocks, never frees --------------------------
  {
    Job *j = novoJob(24, 24 * BASE_MS);
    Trabalho *w = novoTrabalho(j);
    pthread_t ft;
    quadroCustoKB = 0;
    int q0 = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE);
    int s0 = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE);
    memFake(-1);                             // MemAvailable unreadable
    publicar(1, 1, 0, 0, 0, 0);
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_BUSCANDO; __atomic_store_n(&batida, tickFake, __ATOMIC_RELEASE);
    pthread_mutex_unlock(&mu);
    assert(!pthread_create(&ft, NULL, trabalhar, w));
    usleep(1200000);
    CHECK(__atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) == q0,
          "MemAvailable ilegivel: nenhum quadro decodificado (quadros=%d)",
          __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) - q0);
    CHECK(__atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) > s0, "decoder solto com leitura ilegivel");
    memFake(900L * 1024);
    esperar(completoF, j, 15000);
    if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
    pthread_join(ft, NULL);
    CHECK(jGet(j, 0) && jGet(j, 2) == 12, "leitura de volta: a grade termina");
    pthread_mutex_lock(&mu); job = NULL; pthread_mutex_unlock(&mu);
    soltarJob(j);
  }

  // ---- H. display: nearest tile, own time, LRU uploads -------------------------
  {
    Job *j = novoJob(100, 100 * BASE_MS);
    GLuint texs[3]; double cue[3];
    j->passo = DEZ;
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_PRONTO;
    pthread_mutex_unlock(&mu);
    marcarFeito(j, 40);                      // only 200 s has a tile
    quadroVelho = 0;
    CHECK(provQuadros(201.0, 3, texs, cue) == 1 && texs[1], "quadro no ponto exato");
    CHECK(!quadroVelho, "tile proprio nao e 'velho'");
    CHECK(cue[1] == 200.0, "a etiqueta mostra o tempo DO QUADRO (cue=%.1f)", cue[1]);
    quadroVelho = 0;
    CHECK(provQuadros(245.0, 3, texs, cue) == 1, "vizinho a 45 s cobre o buraco");
    CHECK(quadroVelho, "vizinho distante vem marcado como velho");
    CHECK(cue[1] == 200.0, "a etiqueta segue o tempo do vizinho (cue=%.1f)", cue[1]);
    quadroVelho = 0;
    CHECK(provQuadros(560.0, 3, texs, cue) == 1, "a ultima textura segura a tela");
    CHECK(quadroVelho, "fora dos 5 min: qualquer coisa mostrada e velha");
    CHECK(cue[1] == 200.0, "o fallback etiqueta o tempo do tile mostrado (cue=%.1f)", cue[1]);
    // The LRU: pushing more than SLOTS different tiles never grows the set.
    int antes = texGeradas;
    for (int s = 0; s <= 20; s += DEZ) marcarFeito(j, s);
    for (int s = 0; s <= 40; s += DEZ) {
      for (int d = 0; d < 8; d++) provQuadros(s * BASE_MS / 1000.0, 1, texs, cue);
    }
    CHECK(texGeradas - antes <= SLOTS, "uploads limitados a %d texturas (foram %d)", SLOTS, texGeradas - antes);
    int del0 = texDeletadas;
    PROVEDOR.desligar();                     // draw side: the GL context is current
    CHECK(texDeletadas > del0, "desligar libera as texturas do provedor");
    int vivos = 0;
    for (int i = 0; i < SLOTS; i++) if (slots[i].tex) vivos++;
    CHECK(vivos == 0, "nenhum nome GL sobrevive ao desligar (%d restantes)", vivos);
    soltarJob(j);                            // desligar already dropped the job's ref
  }

  // ---- H2. the fallback texture carries its own timestamp -----------------------
  {
    Job *j = novoJob(200, 200 * BASE_MS);    // 1000 s: 510 s is outside the 5-min window
    GLuint texs[3]; double cue[3];
    j->passo = DEZ;
    pthread_mutex_lock(&mu);
    job = j; estado = SEEKR_PRONTO;
    pthread_mutex_unlock(&mu);
    marcarFeito(j, 40);                      // only 200 s has a tile
    provQuadros(200.0, 3, texs, cue);        // its texture goes into a slot
    quadroVelho = 0;
    CHECK(provQuadros(510.0, 3, texs, cue) == 1, "longe de todo tile pronto: a ultima textura segura a tela");
    CHECK(quadroVelho, "fora da janela o mostrado e velho");
    CHECK(cue[1] == 200.0, "o fallback etiqueta o tempo do tile MOSTRADO (cue=%.1f)", cue[1]);
    PROVEDOR.desligar();
    soltarJob(j);
  }

  // ---- I. lifecycle via the provider vtable ------------------------------------
  {
    SeekrFonte f = { .imdb = "tt0000000", .t = 0, .e = 0, .durMs = 120L * 60000,
                     .url = "https://fonte.invalid/filme.mkv", .cabecalhos = "",
                     .tamanho = 0, .videoHash = "", .arquivo = "filme.mkv",
                     .posMs = 0, .playheadMs = 12345, .scrubbing = 0 };
    unsigned g0 = geracao;
    PROVEDOR.pedir(&f);
    CHECK(provEstado() == SEEKR_BUSCANDO, "pedir -> buscando");
    CHECK(geracao != g0, "novo titulo, nova geracao");
#ifdef NV_MINIATURA_SNAP
    {
      MiniEst e = estCopia();
      CHECK(e.playheadMs == 12345, "pedir republica o estado do video para o fio (playhead=%ld)", e.playheadMs);
    }
#endif
    unsigned g1 = geracao;
    PROVEDOR.pedir(&f);
    CHECK(geracao == g1, "o mesmo titulo so bate o heartbeat");
    f.url = "https://fonte.invalid/live.m3u8";
    PROVEDOR.pedir(&f);
    CHECK(provEstado() == SEEKR_SEM_PREVIA, "HLS -> sem previa");
    f.url = "https://fonte.invalid/curto.mkv"; f.durMs = 30000;
    PROVEDOR.pedir(&f);
    CHECK(provEstado() == SEEKR_SEM_PREVIA, "clipe <60 s -> sem previa");
    f.url = "https://fonte.invalid/outro.mkv"; f.durMs = 120L * 60000;
    PROVEDOR.pedir(&f);
    CHECK(provEstado() == SEEKR_BUSCANDO, "titulo novo volta a buscar");
    PROVEDOR.desligar();
    CHECK(provEstado() == SEEKR_DESLIGADO, "desligar -> desligado");
    usleep(600000);                          // the detached workers free their jobs
  }

  printf(falhas ? "FAIL: %d assercoes\n" : "PASS: portao de memoria (regra do guia), grade 5/10 s, ordem, piso e custo medido, cache limitado, vizinho LRU e ciclo de vida\n", falhas);
  return falhas ? 1 : 0;
}
