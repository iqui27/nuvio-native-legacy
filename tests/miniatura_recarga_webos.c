// Directed test for the webOS reload on the .tpk 6+ / webOS seek-thumbnail
// provider (src/miniatura.c, PR 680 stage B, F85). A load revision drops
// video_pronto()/video_tocando() (video.c) and the player's pedir gate
// (`d > 60.0 && video_pronto() && ...`) stops publishing: the worker kept the
// stale pronto=1/tocando=1 and decoded with the decoder open until the 6 s
// heartbeat expired. The production gate AND the publication block are
// extracted verbatim from src/player.c by the .sh into gate.h; prov_pedir,
// miniatura_tv_fonte_estado and the worker are the real ones; the motor, the
// video backend and the network are faked below.
//
//   recarga: after the first tile, pronto/tocando drop with a 2 s buffering.
//   Over 1.2 s (< the 6 s heartbeat) the worker must make ZERO frames and
//   release the decoder; the player-side instrumentation must see ZERO
//   video_*/ajustes_* calls from the worker thread; and the geracao must not
//   move (no new job). Readiness back: the normal gate resumes and the grid
//   finishes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
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
static int motorQuadros, motorSoltadas, motorFechado;
static size_t motorMemoriaBytes = 40L << 20;   // the "next frame" estimate
static long quadroCustoKB;                     // MemAvailable each fake tile eats
static int motorFalhar;
static int motorDecAberto;                     // decoder open? (mini_motor_decoder_aberto)
static int fakePronto = 1, fakeTocando = 1, fakeDts, fakeLargura;
static unsigned fakeBufferando;
static double fakePos;
static const char *fakeUrl = "https://fixture.invalid/test.mkv";
static int redePedidos, redeFalhar;

// video_*/ajustes_* calls made OFF the player thread (the worker must never
// call them itself: off this thread some mutate shared state).
static int contarChamadas, chamadasFora;
static pthread_t dono;
static void registrarChamada(void) {
  if (contarChamadas && !pthread_equal(dono, pthread_self()))
    __atomic_fetch_add(&chamadasFora, 1, __ATOMIC_ACQ_REL);
}

// ----- stub implementations (linked instead of the real ones) -----------------
int perftv_enabled, perftv_uploads, perftv_blur, perftv_glass;
Uint32 SDL_GetTicks(void) { return __atomic_load_n(&tickFake, __ATOMIC_ACQUIRE); }
SDL_RWops *SDL_RWFromConstMem(const void *mem, int size) { (void)mem; (void)size; return (SDL_RWops *)1; }
SDL_Surface *IMG_Load_RW(SDL_RWops *src, int freesrc) { (void)src; (void)freesrc; return NULL; }
SDL_Surface *SDL_ConvertSurfaceFormat(SDL_Surface *src, Uint32 fmt, Uint32 flags) { (void)src; (void)fmt; (void)flags; return NULL; }
int SDL_LockSurface(SDL_Surface *s) { (void)s; return 0; }
void SDL_UnlockSurface(SDL_Surface *s) { (void)s; }
void SDL_FreeSurface(SDL_Surface *s) { if (s) { free(s->format); free(s->pixels); free(s); } }
#undef glTexImage2D
void glTexImage2D(GLenum t, GLint l, GLint i, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const GLvoid *p) {
  (void)t; (void)l; (void)i; (void)w; (void)h; (void)b; (void)f; (void)ty; (void)p;
}
void glGenTextures(GLsizei n, GLuint *t) { for (int i = 0; i < n; i++) t[i] = 1; }
void glDeleteTextures(GLsizei n, const GLuint *t) { (void)n; (void)t; }
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

// The simulated video backend: the driver state changes independently of what
// the player publishes — exactly what the webOS load revision does.
int video_pronto(void) { registrarChamada(); return __atomic_load_n(&fakePronto, __ATOMIC_ACQUIRE); }
int video_tocando(void) { registrarChamada(); return __atomic_load_n(&fakeTocando, __ATOMIC_ACQUIRE); }
unsigned video_bufferando_ms(void) { registrarChamada(); return __atomic_load_n(&fakeBufferando, __ATOMIC_ACQUIRE); }
double video_pos(void) { registrarChamada(); return fakePos; }
int video_dts_estado(void) { registrarChamada(); return __atomic_load_n(&fakeDts, __ATOMIC_ACQUIRE); }
int ajustes_miniatura_largura(void) { registrarChamada(); return __atomic_load_n(&fakeLargura, __ATOMIC_ACQUIRE); }
const char *video_url_atual(void) { registrarChamada(); return fakeUrl; }
const char *video_cabecalhos(void) { registrarChamada(); return ""; }
double video_buffer_fim(void) { registrarChamada(); return 0.0; }

static void publicarVideo(int pronto, int tocando, unsigned bufferandoMs) {
  __atomic_store_n(&fakePronto, pronto, __ATOMIC_RELEASE);
  __atomic_store_n(&fakeTocando, tocando, __ATOMIC_RELEASE);
  __atomic_store_n(&fakeBufferando, bufferandoMs, __ATOMIC_RELEASE);
}

// ----- the player gate's surroundings, faked --------------------------------
// Everything the statements extracted from src/player.c touch, besides the
// video/ajustes stubs above: the item on screen, the stream, the seekr entry
// points (routed to the real provider) and the OpenSeek helpers (inert here).
typedef struct { char imdb[32]; char tipo[16]; } CatItem;
typedef struct { uint64_t tamanhoBytes; char videoHash[24]; char arquivo[1024]; } Stream;
static int epT, epE;
static CatItem fakeItem = { "tt0000000", "movie" };
static Stream fakeStream;
static const CatItem *item(void) { return &fakeItem; }
static int stream_atual(void) { return 0; }
static const Stream *stream_item(int i) { (void)i; return &fakeStream; }
static int ehCanal(void) { return 0; }
int ajustes_seekr_ligado(void) { return 1; }
int seekr_provedor_alternativo(void) { return 1; }
int seekr_provedor_e(const SeekrProvedor *p) { return p == &PROVEDOR; }
static const SeekrProvedor *miniatura_provedor(void) { return &PROVEDOR; }
void seekr_pedir(const char *imdb, int t, int e, long durMs) { (void)imdb; (void)t; (void)e; (void)durMs; }
void seekr_pedir_fonte(const SeekrFonte *f) { PROVEDOR.pedir(f); }
static void openseekAltPasso(const char *imdb, int temporada, int episodio, long durMs, float posSeg) {
  (void)imdb; (void)temporada; (void)episodio; (void)durMs; (void)posSeg;
}
static void openseek_contrib_telemetria(long bufferMs, int rebuffering) { (void)bufferMs; (void)rebuffering; }

#include "gate.h"   // batidaPlayer(): the gate + publication extracted from src/player.c

MiniMotor *mini_motor_abrir(MiniLer ler, void *usuario, int64_t tamanho,
                            char *erro, size_t nerro) {
  (void)usuario; (void)tamanho;
  if (motorFalhar > 1) { snprintf(erro, nerro, "fake: container refused"); return NULL; }
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

// The worker owns these fields under mu; the test reads them the same way.
static int jGet(Job *j, int oque) {
  pthread_mutex_lock(&mu);
  int v = oque == 0 ? j->completo : oque == 1 ? j->vivo : j->feitos;
  pthread_mutex_unlock(&mu);
  return v;
}

int main(void) {
  snprintf(fakeInfo.formato, sizeof fakeInfo.formato, "%s", "matroska");
  snprintf(fakeInfo.codec, sizeof fakeInfo.codec, "%s", "h264");
  fakeInfo.largura = 1920; fakeInfo.altura = 1080; fakeInfo.bits = 8; fakeInfo.chaves = 120;
  snprintf(fakeStream.arquivo, sizeof fakeStream.arquivo, "%s", "test.mkv");
  snprintf(fakeStream.videoHash, sizeof fakeStream.videoHash, "%s", "hash");

  // ---- recarga webOS: readiness drops mid-grid, < the 6 s heartbeat --------
  dono = pthread_self(); contarChamadas = 1;
  quadroCustoKB = 0;
  memFake(900L * 1024);
  publicarVideo(1, 1, 0);                 // ready and playing
  batidaPlayer(120.0, 0.0f, 0);           // the gate opens: pedir creates the job
  Job *j;
  pthread_mutex_lock(&mu);
  j = job;
  if (j) j->refs++;
  unsigned g0 = geracao;
  pthread_mutex_unlock(&mu);
  CHECK(j != NULL, "a primeira batida cria o job");
  if (!j) return 2;
  for (int i = 0; i < 600 && !jGet(j, 2); i++) usleep(5000);
  CHECK(jGet(j, 2) >= 1, "primeiro tile feito antes da recarga");

  // The webOS load revision: pronto/tocando drop, 2 s of buffering reported.
  // The first fallen beat goes out at once — from here on every new frame is
  // the worker decoding blind on a stale state.
  publicarVideo(0, 0, 2000);
  batidaPlayer(120.0, 0.0f, 0);
  int q0 = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE);
  int s0 = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE);
  for (int i = 0; i < 12; i++) {
    usleep(100000);
    __atomic_fetch_add(&tickFake, 100, __ATOMIC_ACQ_REL);
    batidaPlayer(120.0, 0.0f, 0);         // 1.2 s of beats, gate closed
  }
  int q = __atomic_load_n(&motorQuadros, __ATOMIC_ACQUIRE) - q0;
  int s = __atomic_load_n(&motorSoltadas, __ATOMIC_ACQUIRE) - s0;
  int fora = __atomic_load_n(&chamadasFora, __ATOMIC_ACQUIRE);
  printf("recarga: quadros novos=%d soltadas=%d chamadas fora do fio=%d\n", q, s, fora);
  CHECK(q == 0, "recarga: nenhum quadro com a prontidao caida (quadros=%d)", q);
  CHECK(s >= 1, "recarga: o decoder e solto (soltadas=%d)", s);
  CHECK(fora == 0, "o worker nao chama video_*/ajustes_* ele mesmo (fora=%d)", fora);
  pthread_mutex_lock(&mu);
  CHECK(geracao == g0, "nenhum job novo na recarga (geracao %u -> %u)", g0, geracao);
  pthread_mutex_unlock(&mu);

  // Readiness back: the normal gate resumes and the grid finishes.
  publicarVideo(1, 1, 0);
  batidaPlayer(120.0, 0.0f, 0);
  for (int i = 0; i < 600 && !jGet(j, 0); i++) usleep(5000);
  if (!jGet(j, 0)) __atomic_store_n(&j->parar, 1, __ATOMIC_RELEASE);
  CHECK(jGet(j, 0) && jGet(j, 2) == 24, "retomada: a grade termina (feitos=%d)", jGet(j, 2));
  PROVEDOR.desligar();
  for (int i = 0; i < 600 && jGet(j, 1); i++) usleep(5000);
  pthread_mutex_lock(&mu); soltarJob(j); pthread_mutex_unlock(&mu);

  printf(falhas ? "FAIL: %d assercoes\n" : "PASS: recarga webOS solta o decoder sem quadro novo, sem job novo e sem video_* no fio\n", falhas);
  return falhas ? 1 : 0;
}
