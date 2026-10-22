// SEEK-BAR THUMBNAILS MADE ON THE DEVICE (Samsung .tpk, LG webOS, Android;
// miniplat.h). Replaces the Seekr API there: it serves the same seekr_* calls
// player.c and ajustes.c already make (seekr.h), so the drawing code stays as
// it is. seekr.c and seekrquota.c are not built into those targets.
//
// While a title plays, a low-priority thread makes 320x180 JPEGs from the
// nearest keyframes (miniatura_motor.c). Slots are every 5 s; the GRID decides
// which are made (owner, 23/10):
//   5 s  - picture up to 1080p and runtime up to 90:00, or above 1080p and
//          under 70:00 on a device with 2 GB of RAM or more;
//   10 s - everything else, and every MPEG-TS / M2TS file.
// Order on the 10 s grid: scrub point +-30 s -> +-1 min around the playhead ->
// one every 5 min -> +-5% of the runtime -> one every 60 s -> every 10 s.
// Order on the 5 s grid: scrub point +-30 s every 5 s -> +-1 min every 10 s ->
// one every 5 min -> +-1 min every 5 s -> +-5% every 10 s -> one every 60 s ->
// +-5% every 5 s -> whole film every 10 s -> whole film every 5 s.
// Every step goes outwards from its centre, ahead first at equal distance.
// Layered: a 10 s slot is also a 5 s slot, nothing is made twice, and a
// keyframe that serves several slots is decoded once.
// While a slot is not made yet, the seek bar shows the nearest made tile
// (up to 5 min away) with ITS own time, dimmed as "still loading".
//
// MEMORY (owner: never run out of memory, at all costs). Before every frame
// MemAvailable must cover a 150 MB floor plus what the next frame costs
// (measured on the device after the first one). One decoded frame at a time,
// one decoding thread; the decoder stays open between frames (TV tests 22/10:
// ~20% faster) and is closed while paused. The tiles stay compressed in RAM
// (~8 KB each); only the few on screen become textures.
//
// Nothing is kept on the device: with Google Drive linked (minidrive.c) the set
// is looked up before anything is made and saved when the title stops.
//
// Logs `[miniatura]`, never URLs or keys.
#include "seekr.h"
#include "miniplat.h"

#ifdef NV_MINIATURAS
#include "minidrive.h"
#include "miniatura_motor.h"
#include "rede.h"
#include "streams.h"
#include "partes.h"
#include "video.h"
#include "gfx.h"
#include "jpegrapido.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BASE_MS 5000L              // one slot every 5 s; the 10 s grid uses every second one
#define DEZ 2                      // slots per 10 s
// MEMORY GATE (values from the TV tests of 22-23/10, Tizen 9 / 1774 MB):
// MemAvailable must cover the floor plus the next tile's cost. A 4K tile costs
// 35-45 MB of MemAvailable; before one is measured the cost is 60 MB scaled by
// picture size, after it the largest of the last MEDIDAS measured costs x 1.25.
// With the DTS converter running the TV went down to 155 MB with no trouble.
#define PISO_KB (150L * 1024)
#define CUSTO_4K_KB (60L * 1024)
#define MARGEM_PCT 125
#define MEDIDAS 8
#define MEDIDA_MIN_KB (20L * 1024)
#define PAUSA_MEM_MS 1500          // memory re-check while paused
#define INTERVALO_MS 30            // between tiles
#define RAM_5S_4K_MB 2048          // 4K gets the 5 s grid only with this much RAM
#define SCRUB_SLOTS 6              // +-30 s around the scrub point
#define PERTO_SLOTS 12             // +-1 min around the playhead
#define GROSSO_SLOTS 60            // one every 5 min
#define MINUTO_SLOTS 12            // one every 60 s
#define VIZINHO_SLOTS 60           // seek bar: nearest made tile up to 5 min away
#define FATIA 0.05                 // +-5% of the runtime around the playhead
#define SLOTS 4

typedef struct { unsigned char *jpg; int n; long pts; } Tile;

// One title. The display and the worker both hold a reference; the worker
// keeps its own after the title is switched off, so it can still save the set
// to Drive, and frees it last.
typedef struct {
  unsigned g;
  int refs;
  Tile *tiles; unsigned char *falhas; int n;   // n slots of BASE_MS
  int passo;                 // 1 = 5 s grid, DEZ = 10 s grid, 0 = not decided yet
  int feitos, novos;
  long dur;
  volatile int parar;
  int vivo;                  // the worker thread is running
} Job;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static int estado = SEEKR_DESLIGADO;
static unsigned geracao;
static Job *job;                 // the title being shown (NULL = none)
static char urlAtual[4096];      // its link (identity: video_url_atual)
static long durAtual;
static Uint32 batida;            // last seekr_pedir: the player still shows this title

static long alvoScrub = -1; static Uint32 alvoTick;

// Caller holds mu.
static void soltarJob(Job *j) {
  if (!j || --j->refs > 0) return;
  for (int k = 0; k < j->n; k++) free(j->tiles[k].jpg);
  free(j->tiles); free(j->falhas); free(j);
}

// ------------------------------------------------------------------ helpers
static long memDisponivelKB(void) {
  FILE *f = fopen("/proc/meminfo", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (!strncmp(l, "MemAvailable:", 13)) { v = atol(l + 13); break; }
  fclose(f);
  return v;
}
static long memTotalMB(void) {
  FILE *f = fopen("/proc/meminfo", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (!strncmp(l, "MemTotal:", 9)) { v = atol(l + 9) / 1024; break; }
  fclose(f);
  return v;
}
static int nucleos(void) { long n = sysconf(_SC_NPROCESSORS_ONLN); return n > 0 ? (int)n : 1; }
static long rssKB(void) {
  FILE *f = fopen("/proc/self/status", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (!strncmp(l, "VmRSS:", 6)) { v = atol(l + 6); break; }
  fclose(f);
  return v;
}
static void dormirJ(Job *j, int ms) { while (ms > 0 && !j->parar) { int p = ms < 100 ? ms : 100; usleep((useconds_t)p * 1000); ms -= p; } }

// ------------------------------------------------------------------ pack (Drive file)
// "NVMT" u32 version=1, u32 grid ms, u32 n slots, u32 count, then per tile
// u32 slot, i32 pts ms, u32 len, JPEG bytes. Little-endian. The grid is the
// title's own (5000 or 10000) and slots count in it, so sets saved by the
// first 10 s-only builds read the same way.
static void put32(unsigned char *p, uint32_t v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }
static uint32_t get32(const unsigned char *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

static unsigned char *empacotar(Job *j, size_t *n, int *conta) {
  size_t tam = 20;
  int c = 0;
  pthread_mutex_lock(&mu);
  int ps = j->passo > 0 ? j->passo : DEZ;
  for (int k = 0; k < j->n; k += ps) if (j->tiles[k].jpg) { tam += 12 + (size_t)j->tiles[k].n; c++; }
  unsigned char *b = c ? malloc(tam) : NULL;
  if (b) {
    unsigned char *p = b;
    memcpy(p, "NVMT", 4); put32(p + 4, 1); put32(p + 8, (uint32_t)(ps * BASE_MS));
    put32(p + 12, (uint32_t)((j->n + ps - 1) / ps)); put32(p + 16, (uint32_t)c);
    p += 20;
    for (int k = 0; k < j->n; k += ps) if (j->tiles[k].jpg) {
      put32(p, (uint32_t)(k / ps)); put32(p + 4, (uint32_t)j->tiles[k].pts); put32(p + 8, (uint32_t)j->tiles[k].n);
      memcpy(p + 12, j->tiles[k].jpg, (size_t)j->tiles[k].n); p += 12 + j->tiles[k].n;
    }
  }
  pthread_mutex_unlock(&mu);
  *n = b ? tam : 0; *conta = c;
  return b;
}

// Adds the tiles of a pack to the set (never replaces one already made).
// Count added; *grade = the pack's grid in ms (0 = not a pack).
static int desempacotar(Job *jb, const unsigned char *b, size_t n, long *grade) {
  if (grade) *grade = 0;
  if (n < 20 || memcmp(b, "NVMT", 4) || get32(b + 4) != 1) return 0;
  uint32_t g = get32(b + 8);
  if (g != BASE_MS && g != DEZ * BASE_MS) return 0;
  int mult = (int)(g / BASE_MS);
  if (grade) *grade = (long)g;
  uint32_t c = get32(b + 16);
  size_t pos = 20;
  int add = 0;
  pthread_mutex_lock(&mu);
  for (uint32_t i = 0; i < c && pos + 12 <= n; i++) {
    uint32_t k = get32(b + pos) * (uint32_t)mult, len = get32(b + pos + 8);
    long pts = (long)(int32_t)get32(b + pos + 4);
    if (pos + 12 + len > n || len < 125 || len > 200000) break;
    if ((int)k < jb->n && !jb->tiles[k].jpg) {
      unsigned char *j = malloc(len);
      if (j) { memcpy(j, b + pos + 12, len); jb->tiles[k] = (Tile){ j, (int)len, pts }; jb->feitos++; add++; }
    }
    pos += 12 + len;
  }
  if (add && jb == job) estado = SEEKR_PRONTO;
  pthread_mutex_unlock(&mu);
  return add;
}

// ------------------------------------------------------------------ order of work
static int pronto(const Job *j, int k) { return k >= 0 && k < j->n && j->tiles[k].jpg; }
static int livre(const Job *j, int k) {
  return k >= 0 && k < j->n && j->passo > 0 && k % j->passo == 0 && !j->tiles[k].jpg && j->falhas[k] < 2;
}
// Nearest free slot on the `passo` grid within `raio` slots of `c`, ahead first.
static int perto(const Job *j, int c, int raio, int passo) {
  for (int d = 0; d <= raio; d++) {
    if ((c + d) % passo == 0 && livre(j, c + d)) return c + d;
    if (d && c - d >= 0 && (c - d) % passo == 0 && livre(j, c - d)) return c - d;
  }
  return -1;
}

enum { E_SCRUB, E_PERTO, E_FATIA, E_TODO };
typedef struct { int tipo, passo; } Etapa;
// 10 s grid: scrub +-30 s, +-1 min, every 5 min, +-5%, every 60 s, every 10 s.
static const Etapa ORDEM_10[] = {
  { E_SCRUB, DEZ }, { E_PERTO, DEZ }, { E_TODO, GROSSO_SLOTS }, { E_FATIA, DEZ },
  { E_TODO, MINUTO_SLOTS }, { E_TODO, DEZ } };
// 5 s grid (owner, 23/10): scrub +-30 s every 5 s, +-1 min every 10 s, every
// 5 min, +-1 min every 5 s, +-5% every 10 s, every 60 s, +-5% every 5 s,
// whole film every 10 s, whole film every 5 s.
static const Etapa ORDEM_5[] = {
  { E_SCRUB, 1 }, { E_PERTO, DEZ }, { E_TODO, GROSSO_SLOTS }, { E_PERTO, 1 }, { E_FATIA, DEZ },
  { E_TODO, MINUTO_SLOTS }, { E_FATIA, 1 }, { E_TODO, DEZ }, { E_TODO, 1 } };

// Next slot to make (-1 = none left). *classe = the step (1-based). Caller holds mu.
static int proximo(const Job *j, long phMs, int *classe) {
  const Etapa *o = j->passo == 1 ? ORDEM_5 : ORDEM_10;
  int no = j->passo == 1 ? (int)(sizeof ORDEM_5 / sizeof *ORDEM_5) : (int)(sizeof ORDEM_10 / sizeof *ORDEM_10);
  int ph = (int)((phMs + BASE_MS / 2) / BASE_MS), fatia = (int)(j->dur * FATIA / BASE_MS) + 1;
  int scrub = alvoScrub >= 0 && (Sint32)(SDL_GetTicks() - alvoTick) < 4000;
  int k0 = (int)((alvoScrub + BASE_MS / 2) / BASE_MS);
  if (j->passo <= 0) return -1;
  if (ph >= j->n) ph = j->n - 1;
  if (ph < 0) ph = 0;
  for (int e = 0; e < no; e++) {
    int k = -1;
    switch (o[e].tipo) {
      case E_SCRUB: if (scrub) k = perto(j, k0, SCRUB_SLOTS, o[e].passo); break;
      case E_PERTO: k = perto(j, ph, PERTO_SLOTS, o[e].passo); break;
      case E_FATIA: k = perto(j, ph, fatia, o[e].passo); break;
      default:      k = perto(j, ph, j->n, o[e].passo); break;
    }
    if (k >= 0) { *classe = e + 1; return k; }
  }
  return -1;
}

// The grid for this file (see the top of the file).
static int gradeDe(const MiniInfo *i, long durMs, const char **porque) {
  long ram = memTotalMB();
  int hd = i->largura <= 2048 && i->altura <= 1152;
  if (!strcmp(i->formato, "mpegts")) { *porque = "M2TS/TS"; return DEZ; }
  if (hd) {
    if (durMs <= 90L * 60000) { *porque = "up to 1080p, 90 min or shorter"; return 1; }
    *porque = "up to 1080p, longer than 90 min"; return DEZ;
  }
  if (durMs >= 70L * 60000) { *porque = "above 1080p, 70 min or longer"; return DEZ; }
  if (ram >= 0 && ram < RAM_5S_4K_MB) { *porque = "above 1080p, under 2 GB of RAM"; return DEZ; }
  *porque = "above 1080p, shorter than 70 min";
  return 1;
}

typedef struct {
  Job *j;
  char url[4096], cab[2048];
  char nome[512]; uint64_t bytes;
  char imdb[24]; int t, e;
} Trabalho;

// The engine needs FFmpeg: the .tpk 6+ and LG link it for the DTS converter
// (NV_DTS_FFMPEG), Android links a thumbnail-only build (NV_MINI_FFMPEG,
// tools/android/ffmpeg-miniaturas.sh). NuvioTpk40 has none.
#if defined(NV_DTS_FFMPEG) || defined(NV_MINI_FFMPEG)
// ------------------------------------------------------------------ source reader
typedef struct {
  char url[4096], cab[2048], final[4096];
  int cruzou;                    // the redirect left the original origin: no headers
  int64_t total;
  int falhas;
  Job *j;
} Leitor;

static long lerFonte(void *u, int64_t ini, int64_t fim, unsigned char *dst) {
  Leitor *l = u;
  for (int tent = 0; tent < 3 && !l->j->parar; tent++) {
    long n = 0; int64_t total = -1; int st = 0, cross = 0;
    char fin[4096] = "";
    const char *url = l->final[0] ? l->final : l->url;
    const char *cab = l->final[0] && l->cruzou ? "" : l->cab;
    char *b = rede_baixar_trecho64_final(url, cab, ini, fim, &n, &total, &st, &l->j->parar, fin, &cross);
    if (b && n > 0) {
      if (!l->final[0] && fin[0]) { snprintf(l->final, sizeof l->final, "%s", fin); l->cruzou = cross; }
      if (total > 0) l->total = total;
      long k = n < (long)(fim - ini + 1) ? n : (long)(fim - ini + 1);
      memcpy(dst, b, (size_t)k);
      free(b);
      l->falhas = 0;
      return k;
    }
    free(b);
    // A CDN link that expired: resolve the original link again.
    if (l->final[0] && (st == 401 || st == 403 || st == 404 || st == 410)) { l->final[0] = 0; continue; }
    if (st == 0) dormirJ(l->j, 500 * (tent + 1));
  }
  l->falhas++;
  return -1;
}

static void salvarDrive(Job *j, const MiniMeta *meta, const char *id) {
  size_t n; int c;
  if (!minidrive_ligado()) return;
  pthread_mutex_lock(&mu); int ha = j->novos; pthread_mutex_unlock(&mu);
  if (!ha) { if (id[0]) minidrive_tocar(id); return; }
  unsigned char *b = empacotar(j, &n, &c);
  if (!b) return;
  MiniMeta m = *meta; m.quadros = c;
  if (minidrive_enviar(id, &m, b, n) == 0) { pthread_mutex_lock(&mu); j->novos = 0; pthread_mutex_unlock(&mu); }
  free(b);
}

static void *trabalhar(void *u) {
  Trabalho *w = u;
  Job *j = w->j;
  Leitor *l = calloc(1, sizeof *l);
  MiniMotor *m = NULL;
  MiniMeta meta; memset(&meta, 0, sizeof meta);
  char id[128] = "", erro[128] = "";
  // One decoding thread: on the TV 2-3 threads were no faster while the player
  // decodes 4K (tests 22/10) and cost more memory.
  const int threads = 1;
  int seguidas = 0, feitosAqui = 0;
  unsigned long somaMs = 0;
  long medidas[MEDIDAS] = {0}; int nMedidas = 0;
  Uint32 ultimaPausaLog = 0; int pausasCaladas = 0;
  long gradePack = 0;
  if (!l) goto fim;
  l->j = j;
  snprintf(l->url, sizeof l->url, "%s", w->url);
  snprintf(l->cab, sizeof l->cab, "%s", w->cab);
  meta.durMs = j->dur;
  snprintf(meta.imdb, sizeof meta.imdb, "%s", w->imdb);
  meta.temporada = w->t; meta.episodio = w->e;
  minidrive_chave_nome(w->nome, w->bytes, meta.nk);

  // Let the player settle first.
  for (int k = 0; k < 30 && !j->parar && !video_pronto(); k++) dormirJ(j, 500);
  dormirJ(j, 500);
  if (j->parar) goto fim;

  // Fingerprint: two 64 KiB reads.
  {
    unsigned char *cab = malloc(65536), *cau = malloc(65536);
    long a = cab && cau ? lerFonte(l, 0, 65535, cab) : -1;
    long b = -1;
    if (a > 0 && l->total > 65536) b = lerFonte(l, l->total - 65536, l->total - 1, cau);
    else if (a > 0) { memcpy(cau, cab, (size_t)a); b = a; }
    if (a > 0 && b > 0 && l->total > 0) {
      mini_impressao((uint64_t)l->total, cab, (size_t)a, cau, (size_t)b, meta.fp);
      if (!meta.nk[0]) minidrive_chave_nome(w->nome, (uint64_t)l->total, meta.nk);
    }
    free(cab); free(cau);
    printf("[miniatura] source: %lld MB, fingerprint %s, file name %s\n", (long long)(l->total / 1048576),
           meta.fp[0] ? "read" : "unreadable", meta.nk[0] ? "known" : "unknown");
    fflush(stdout);
  }

  // Google Drive first: a saved set means nothing to make.
  if (minidrive_ligado() && (meta.fp[0] || meta.nk[0]) && !j->parar) {
    unsigned char *b = NULL; size_t n = 0;
    if (minidrive_buscar(&meta, &b, &n, id, sizeof id) == 1) {
      int add = desempacotar(j, b, n, &gradePack);
      printf("[miniatura] %d tile(s) from Google Drive (%ld s grid)\n", add, gradePack / 1000); fflush(stdout);
      minidrive_tocar(id);
    }
    free(b);
    minidrive_limpeza();
  }
  // A complete 5 s set: nothing left to make, not even opening the file.
  pthread_mutex_lock(&mu); int completo = gradePack == BASE_MS && j->feitos >= j->n; pthread_mutex_unlock(&mu);
  if (completo || j->parar) goto fim;
  if (l->total <= 0) { printf("[miniatura] stopped: source size unknown\n"); goto semPrevia; }

  m = mini_motor_abrir(lerFonte, l, l->total, erro, sizeof erro);
  if (!m) { printf("[miniatura] not made for this file: %s\n", erro); goto semPrevia; }
  {
    const MiniInfo *i = mini_motor_info(m);
    const char *porque = "";
    int ps = gradeDe(i, j->dur, &porque);
    pthread_mutex_lock(&mu);
    j->passo = ps;
    pthread_mutex_unlock(&mu);
    printf("[miniatura] %s %s %dx%d %d-bit%s, %d keyframes; %s grid (%s), %d slots\n",
           i->formato, i->codec, i->largura, i->altura, i->bits, i->hdr ? " HDR" : "", i->chaves,
           ps == 1 ? "5 s" : "10 s", porque, (j->n + ps - 1) / ps);
    fflush(stdout);
  }

  while (!j->parar) {
    // Pauses: the player is not showing this title, or is buffering / opening.
    if ((Sint32)(SDL_GetTicks() - batida) > 6000) { mini_motor_soltar_decoder(m); dormirJ(j, 500); continue; }
    if (video_bufferando_ms() > 0 || !video_pronto()) { mini_motor_soltar_decoder(m); dormirJ(j, 700); continue; }
    // Memory before every frame.
    long disp = memDisponivelKB();
    long precisa = (long)(mini_motor_memoria(m, threads) / 1024);
    {
      // Guess until measured: 60 MB for a 4K frame, scaled by picture size.
      const MiniInfo *mi = mini_motor_info(m);
      double px = (double)mi->largura * (double)mi->altura / (3840.0 * 2160.0);
      long g = (long)(CUSTO_4K_KB * (px > 0.05 ? px : 0.05));
      if (g < precisa) precisa = g;
    }
    if (nMedidas >= 1) {
      long maior = 0;
      for (int i = 0; i < nMedidas && i < MEDIDAS; i++) if (medidas[i] > maior) maior = medidas[i];
      maior = maior * MARGEM_PCT / 100;
      if (maior < MEDIDA_MIN_KB) maior = MEDIDA_MIN_KB;
      precisa = maior;
    }
    if (disp >= 0 && disp < PISO_KB + precisa) {
      int dts = video_dts_estado() == VIDEO_DTS_PREPARANDO || video_dts_estado() == VIDEO_DTS_CONVERTIDO;
      mini_motor_soltar_decoder(m);
      // One line when it starts, then at most one every 30 s.
      if (!ultimaPausaLog || SDL_GetTicks() - ultimaPausaLog > 30000) {
        printf("[miniatura] paused: low memory (free %ld MB, needs %ld MB + floor %ld MB%s)%s\n",
               disp / 1024, precisa / 1024, PISO_KB / 1024, dts ? ", DTS converting" : "", pausasCaladas ? ", still waiting" : "");
        fflush(stdout);
        ultimaPausaLog = SDL_GetTicks(); pausasCaladas = 0;
      } else pausasCaladas++;
      dormirJ(j, PAUSA_MEM_MS);
      continue;
    }
    ultimaPausaLog = 0; pausasCaladas = 0;

    int classe = 0, k, k2 = -1, c2 = 0;
    long ph = (long)(video_pos() * 1000.0);
    pthread_mutex_lock(&mu);
    k = proximo(j, ph, &classe);
    if (k >= 0) {
      // The slot after this one, for the prefetch: same order, this one taken.
      unsigned char f0 = j->falhas[k];
      j->falhas[k] = 99;
      k2 = proximo(j, ph, &c2);
      j->falhas[k] = f0;
    }
    long tol = (long)j->passo * BASE_MS / 2;
    pthread_mutex_unlock(&mu);
    if (k < 0) break;
    long alvo = k * BASE_MS;
    if (alvo > j->dur - 1) alvo = j->dur - 1;
    {
      long alvo2 = k2 >= 0 ? k2 * BASE_MS : -1;
      if (alvo2 > j->dur - 1) alvo2 = j->dur - 1;
      mini_motor_proximo(m, alvo2, tol);
    }

    // Keyframe shared with a slot already made: copy it, no decode.
    long kf = mini_motor_chave(m, alvo, tol);
    int irmao = -1;
    if (kf >= 0) {
      pthread_mutex_lock(&mu);
      for (int d = 1; d <= 6 && irmao < 0; d++) {
        if (pronto(j, k - d) && labs(j->tiles[k - d].pts - kf) < 50) irmao = k - d;
        else if (pronto(j, k + d) && labs(j->tiles[k + d].pts - kf) < 50) irmao = k + d;
      }
      if (irmao >= 0) {
        unsigned char *c = malloc((size_t)j->tiles[irmao].n);
        if (c) { memcpy(c, j->tiles[irmao].jpg, (size_t)j->tiles[irmao].n);
                 j->tiles[k] = (Tile){ c, j->tiles[irmao].n, j->tiles[irmao].pts }; j->feitos++; j->novos++; }
        else irmao = -1;
      }
      pthread_mutex_unlock(&mu);
      if (irmao >= 0) continue;
    }

    unsigned char *jpg = NULL; int nj = 0; MiniQuadro q;
    long antes = memDisponivelKB();
    if (mini_motor_quadro(m, alvo, tol, threads, &j->parar, &jpg, &nj, &q, erro, sizeof erro) < 0) {
      if (j->parar) break;
      printf("[miniatura] slot %d (%lds) failed: %s\n", k, alvo / 1000, erro); fflush(stdout);
      pthread_mutex_lock(&mu); j->falhas[k]++; pthread_mutex_unlock(&mu);
      if (++seguidas >= 3) {
        printf("[miniatura] 3 failures in a row: waiting 30 s\n"); fflush(stdout);
        mini_motor_soltar_decoder(m); dormirJ(j, 30000); seguidas = 0;
      }
      if (l->falhas >= 6) { printf("[miniatura] source unreachable: stopped\n"); break; }
      continue;
    }
    seguidas = 0;
    long depois = memDisponivelKB();
    pthread_mutex_lock(&mu);
    if (!j->tiles[k].jpg) {
      j->tiles[k] = (Tile){ jpg, nj, q.ptsMs }; jpg = NULL; j->feitos++; j->novos++;
      if (j == job) estado = SEEKR_PRONTO;
    }
    int f = j->feitos, n = (j->n + j->passo - 1) / j->passo;
    pthread_mutex_unlock(&mu);
    free(jpg);
    feitosAqui++; somaMs += q.totalMs;
    // One line per tile while the first window fills, then one every 20.
    if (feitosAqui <= 20 || feitosAqui % 20 == 0) {
      printf("[miniatura] slot %ld s (step %d) frame %lds decode %ums total %ums, read %ld KB in %d request(s)",
             alvo / 1000, classe, q.ptsMs / 1000, q.decodMs, q.totalMs, q.bytesLidos / 1024, q.pedidos);
      if (q.adiantadoKB) printf(" + %ld KB ahead", q.adiantadoKB);
      printf(", free %ld->%ld MB, rss %ld MB, %d/%d done\n", antes / 1024, depois / 1024, rssKB() / 1024, f, n);
    }
    fflush(stdout);
    // What this tile really cost (MemAvailable before -> after, decoder open).
    if (antes >= 0 && depois >= 0 && antes > depois) medidas[nMedidas++ % MEDIDAS] = antes - depois;
    dormirJ(j, INTERVALO_MS);
  }
  if (feitosAqui)
    printf("[miniatura] made %d tile(s), %lu ms average, %lld MB read\n", feitosAqui,
           somaMs / (unsigned long)feitosAqui, mini_motor_lidos(m) / 1048576);
  goto fim;
semPrevia:
  pthread_mutex_lock(&mu);
  if (j == job && !j->feitos) estado = SEEKR_SEM_PREVIA;
  pthread_mutex_unlock(&mu);
fim:
  if (m) mini_motor_fechar(m);
  fflush(stdout);
  salvarDrive(j, &meta, id);
  free(l);
  pthread_mutex_lock(&mu);
  j->vivo = 0;
  soltarJob(j);
  pthread_mutex_unlock(&mu);
  free(w);
  return NULL;
}
#else
// NuvioTpk40 (no FFmpeg): nothing can be decoded.
static void *trabalhar(void *u) {
  Trabalho *w = u;
  pthread_mutex_lock(&mu);
  if (w->j == job) estado = SEEKR_SEM_PREVIA;
  w->j->vivo = 0;
  soltarJob(w->j);
  pthread_mutex_unlock(&mu);
  free(w);
  return NULL;
}
#endif

// ------------------------------------------------------------------ title lifecycle
static void nomeDaFonte(char *nome, size_t n, uint64_t *bytes) {
  nome[0] = 0; *bytes = 0;
  const PartesLista *pl = partes_sessao_lista();
  int k = partes_sessao_indice();
  if (pl && k >= 0 && k < pl->n && pl->arq) {      // a file opened inside a source (TorBox's name)
    snprintf(nome, n, "%s", pl->arq[k].nome); *bytes = pl->arq[k].bytes; return;
  }
  const Stream *s = stream_item(stream_atual());
  if (s) { snprintf(nome, n, "%s", s->arquivo); *bytes = s->tamanhoBytes; }
}

void seekr_pedir(const char *imdb, int t, int e, long durMs) {
  const char *url = video_url_atual();
  Trabalho *w;
  Job *j;
  pthread_t f;
  if (!url || !*url) return;
  pthread_mutex_lock(&mu);
  batida = SDL_GetTicks();
  // Same title still on screen: only the heartbeat.
  if (urlAtual[0] && !strcmp(urlAtual, url) && labs(durAtual - durMs) < 3000) { pthread_mutex_unlock(&mu); return; }
  pthread_mutex_unlock(&mu);
  seekr_desligar();                        // the previous title's worker saves its set and ends
  pthread_mutex_lock(&mu);
  snprintf(urlAtual, sizeof urlAtual, "%s", url);
  durAtual = durMs;
  batida = SDL_GetTicks();
  if (!strncmp(url, "http://127.0.0.1", 16) || strstr(url, ".m3u8") || durMs < 60000) {
    estado = SEEKR_SEM_PREVIA; pthread_mutex_unlock(&mu); return;
  }
  pthread_mutex_unlock(&mu);
  j = calloc(1, sizeof *j);
  w = calloc(1, sizeof *w);
  int n = (int)((durMs - 1) / BASE_MS + 1);
  if (j) { j->tiles = calloc((size_t)n, sizeof *j->tiles); j->falhas = calloc((size_t)n, 1); }
  if (!j || !w || !j->tiles || !j->falhas) {
    if (j) { free(j->tiles); free(j->falhas); }
    free(j); free(w);
    return;
  }
  j->n = n; j->dur = durMs; j->refs = 2; j->vivo = 1;
  w->j = j;
  snprintf(w->url, sizeof w->url, "%s", url);
  snprintf(w->cab, sizeof w->cab, "%s", video_cabecalhos() ? video_cabecalhos() : "");
  nomeDaFonte(w->nome, sizeof w->nome, &w->bytes);
  snprintf(w->imdb, sizeof w->imdb, "%s", imdb ? imdb : "");
  w->t = t; w->e = e;
  pthread_mutex_lock(&mu);
  j->g = ++geracao;
  job = j;
  estado = SEEKR_BUSCANDO;
  alvoScrub = -1;
  pthread_mutex_unlock(&mu);
  {
    static int dispositivo;          // once per run: what this device has
    if (!dispositivo) {
      dispositivo = 1;
      printf("[miniatura] device: RAM %ld MB, %d CPU core(s), MemAvailable %ld MB\n",
             memTotalMB(), nucleos(), memDisponivelKB() / 1024);
    }
  }
  printf("[miniatura] new title: %ld min%s\n", durMs / 60000, minidrive_ligado() ? ", Google Drive linked" : "");
  fflush(stdout);
  if (pthread_create(&f, NULL, trabalhar, w) == 0) pthread_detach(f);
  else {
    pthread_mutex_lock(&mu);
    j->vivo = 0; soltarJob(j);            // the worker's reference
    estado = SEEKR_SEM_PREVIA;
    pthread_mutex_unlock(&mu);
    free(w);
  }
}

void seekr_desligar(void) {
  // The worker keeps its own reference: it saves the set to Drive and frees it.
  pthread_mutex_lock(&mu);
  if (job) { job->parar = 1; soltarJob(job); job = NULL; }
  geracao++;
  urlAtual[0] = 0; durAtual = 0;
  estado = SEEKR_DESLIGADO;
  pthread_mutex_unlock(&mu);
}

// ------------------------------------------------------------------ seekr.h surface
int seekr_estado(void) { int r; pthread_mutex_lock(&mu); r = estado; pthread_mutex_unlock(&mu); return r; }
int seekr_tem_chave(void) { return 1; }          // nothing to configure: made on this TV
void seekr_definir_chave(const char *c) { (void)c; }
void seekr_chave_propria(int p) { (void)p; }
int seekr_validar(const char *c) { (void)c; return 1; }
void seekr_tentar_novamente(void) {}
void seekr_definir_ajuste_ms(long ms) { (void)ms; }   // frames of this very file: no sync offset
void seekr_ocioso(void) {}
void seekr_uso(SeekrUso *u) { if (u) memset(u, 0, sizeof *u); }
const char *seekr_estado_rotulo(int est) {
  switch (est) {
    case SEEKR_BUSCANDO: return "Buscando miniaturas";
    case SEEKR_PRONTO: return "Miniaturas disponíveis";
    case SEEKR_SEM_PREVIA: return "Este título não tem miniaturas";
    default: return "Miniaturas não solicitadas";
  }
}
int seekr_horario_local(long long utc, char *saida, size_t tamanho) {
  struct tm local; time_t t = (time_t)utc;
  if (!saida || !tamanho) return 0;
  saida[0] = 0;
  if (utc <= 0 || !localtime_r(&t, &local)) return 0;
  return strftime(saida, tamanho, "%d/%m %H:%M", &local) != 0;
}

// Display side (draw thread only): a few textures keyed by slot.
typedef struct { GLuint tex; int slot; unsigned g; Uint32 uso; } Slot;
static Slot slots[SLOTS];
static Uint32 relogioUso;
static int quadroVelho;
int seekr_quadro_velho(void) { return quadroVelho; }

static int slotTex(int k, unsigned g) {
  for (int i = 0; i < SLOTS; i++) if (slots[i].tex && slots[i].slot == k && slots[i].g == g) return i;
  return -1;
}

// Uploads tile k (JPEG -> texture), keeping the slots in `proteger`. Draw thread.
static void subir(int k, unsigned g, const int *proteger, int np) {
  unsigned char *copia = NULL; int n = 0;
  pthread_mutex_lock(&mu);
  if (job && job->g == g && pronto(job, k) && (copia = malloc((size_t)job->tiles[k].n)) != NULL) {
    memcpy(copia, job->tiles[k].jpg, (size_t)job->tiles[k].n); n = job->tiles[k].n;
  }
  pthread_mutex_unlock(&mu);
  if (!copia) return;
  int w0 = 0, h0 = 0;
  SDL_Surface *s = jpeg_rapido_carregar_mem(copia, (size_t)n, 1 << 20, &w0, &h0);
  if (!s) { SDL_RWops *rw = SDL_RWFromConstMem(copia, n); s = rw ? IMG_Load_RW(rw, 1) : NULL; }
  free(copia);
  if (!s) return;
  if (s->format->format != SDL_PIXELFORMAT_ABGR8888) {
    SDL_Surface *c = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(s); s = c;
    if (!s) return;
  }
  int alvo = -1;
  for (int i = 0; i < SLOTS; i++) {
    int prot = 0;
    for (int q = 0; q < np; q++) if (slots[i].tex && slots[i].g == g && slots[i].slot == proteger[q]) prot = 1;
    if (prot) continue;
    if (!slots[i].tex) { alvo = i; break; }
    if (alvo < 0 || slots[i].uso < slots[alvo].uso) alvo = i;
  }
  if (alvo < 0) alvo = 0;
  if (!slots[alvo].tex) glGenTextures(1, &slots[alvo].tex);
  glBindTexture(GL_TEXTURE_2D, slots[alvo].tex);
  if (SDL_MUSTLOCK(s)) SDL_LockSurface(s);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s->w, s->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, s->pixels);
  if (SDL_MUSTLOCK(s)) SDL_UnlockSurface(s);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  SDL_FreeSurface(s);
  slots[alvo].slot = k; slots[alvo].g = g; slots[alvo].uso = ++relogioUso;
}

// Nearest slot with a tile within `raio` slots of k (-1 none). Caller holds mu.
static int vizinho(const Job *j, int k, int raio) {
  for (int d = 0; d <= raio; d++) {
    if (pronto(j, k + d)) return k + d;
    if (d && pronto(j, k - d)) return k - d;
  }
  return -1;
}

int seekr_quadros(double posSeg, int n, GLuint *texs, double *cueSeg) {
  int quer[3], nq = 0, i, achou = 0, atualK = -1, antes = -1, depois = -1, longe = 0;
  unsigned g = 0;
  if (n < 1) return 0;
  if (n > 3) n = 3;
  for (i = 0; i < n; i++) { texs[i] = 0; if (cueSeg) cueSeg[i] = -1.0; }
  long pos = (long)(posSeg * 1000.0);
  pthread_mutex_lock(&mu);
  // The person is looking here: it becomes the first thing to make.
  alvoScrub = pos; alvoTick = SDL_GetTicks();
  if (!job) { pthread_mutex_unlock(&mu); return 0; }
  g = job->g;
  int k = (int)((pos + BASE_MS / 2) / BASE_MS);
  if (k >= job->n) k = job->n - 1;
  if (k < 0) k = 0;
  atualK = vizinho(job, k, VIZINHO_SLOTS);          // nearest made tile, up to 5 min away
  // Dimmed when it is not this spot's own slot on the title's grid.
  longe = atualK >= 0 && abs(atualK - k) > (job->passo > 0 ? job->passo : DEZ);
  if (atualK >= 0) {
    // Strip neighbours: the nearest made tile on each side.
    for (int d = 1; d <= VIZINHO_SLOTS && antes < 0; d++) if (pronto(job, atualK - d)) antes = atualK - d;
    for (int d = 1; d <= VIZINHO_SLOTS && depois < 0; d++) if (pronto(job, atualK + d)) depois = atualK + d;
    quer[nq++] = atualK;
    if (n > 1 && antes >= 0) quer[nq++] = antes;
    if (n > 1 && depois >= 0) quer[nq++] = depois;
    if (cueSeg) {
      if (n == 1) cueSeg[0] = job->tiles[atualK].pts / 1000.0;
      else {
        cueSeg[0] = antes >= 0 ? job->tiles[antes].pts / 1000.0 : -1.0;
        cueSeg[1] = job->tiles[atualK].pts / 1000.0;
        cueSeg[2] = depois >= 0 ? job->tiles[depois].pts / 1000.0 : -1.0;
      }
    }
  }
  pthread_mutex_unlock(&mu);
  // At most one JPEG decode per call (320x180: a few ms): the current one first.
  for (i = 0; i < nq; i++) if (slotTex(quer[i], g) < 0) { subir(quer[i], g, quer, nq); break; }
  quadroVelho = longe;                               // not this spot's own tile yet: shown dimmed
  if (atualK >= 0) {
    int ordem[3] = { n == 1 ? atualK : antes, atualK, depois };
    int no = n == 1 ? 1 : 3;
    for (i = 0; i < no; i++) {
      int sl = ordem[i] >= 0 ? slotTex(ordem[i], g) : -1;
      if (sl >= 0) { texs[i] = slots[sl].tex; slots[sl].uso = ++relogioUso; if (ordem[i] == atualK) achou = 1; }
    }
  }
  if (!achou) {
    // Not made yet (or still uploading): the last one shown, marked as old.
    int m = -1, c = n == 1 ? 0 : 1;
    for (i = 0; i < SLOTS; i++) if (slots[i].tex && slots[i].g == g && (m < 0 || slots[i].uso > slots[m].uso)) m = i;
    if (m >= 0) { texs[c] = slots[m].tex; achou = 1; quadroVelho = 1; }
  }
  return achou;
}

GLuint seekr_quadro(double posSeg, double *cueSeg) {
  GLuint t = 0;
  seekr_quadros(posSeg, 1, &t, cueSeg);
  return t;
}

#ifdef NV_SHOT_HOOKS
void seekr_shot(int est, const GLuint *t, const double *cue, int velho) { (void)est; (void)t; (void)cue; (void)velho; }
#endif

#endif // NV_MINIATURAS
