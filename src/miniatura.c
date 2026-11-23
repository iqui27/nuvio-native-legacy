// SEEK-BAR THUMBNAILS MADE ON THE DEVICE (Samsung .tpk 6+ and LG webOS;
// miniplat.h). Ported from PR 680's src/miniatura.c, adapted to the provider
// interface of seekr.h (SeekrProvedor): the player and Ajustes keep calling
// the same seekr_* entry points, and seekr.c forwards to this provider when
// the setting "Provedor das miniaturas" is "No aparelho". The .wgt
// (WebAssembly), the desktop, the NuvioTpk40 (no FFmpeg) and packages without
// the DTS FFmpeg keep the Seekr providers.
//
// While a title plays, a low-priority thread makes 320x180 JPEGs from the
// nearest keyframes (miniatura_motor.c). Slots are every 5 s; the GRID decides
// which are made (owner, 23/10):
//   5 s  - picture up to 1080p and runtime up to 90:00, or above 1080p and
//          under 70:00 with at least 2 GB of RAM (unknown RAM = the 10 s grid,
//          unlike the ref, which allowed 5 s there);
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
// The whole feature only exists where the device can take it: the package
// must link the motor's FFmpeg (decoders + MJPEG encoder + mpegts demuxer —
// mini_motor_tem_ferramentas) AND memory must be enough by the SAME rule as
// the TV guide (memoria.h / epg_memoria_eh_baixa: forced Baixa, or Auto with
// unknown or <= 2048 MB RAM = off; __EMSCRIPTEN__ Auto = always off —
// miniatura_tv_capaz). The RAM source is the one of ajEpgMemoriaAplicar:
// tex_orcamento_info(), fallback SDL_GetSystemRAM().
//
// MEMORY (owner: never run out of memory, at all costs). Before every frame
// MemAvailable must cover a 150 MB floor plus what the next frame costs
// (measured on the device after the first one). One decoded frame at a time,
// one decoding thread; the decoder stays open between frames (TV tests 22/10:
// ~20% faster) and is closed while paused. The tiles stay compressed in RAM
// (~8 KB each); only the few on screen become textures (SLOTS slots, LRU).
//
// Nothing is kept on the device between plays (Google Drive is NOT in this
// port — the owner declined it for 2.1). Logs `[miniatura]`, never URLs or
// keys.
#include "seekr.h"
#include "miniplat.h"

#if defined(NV_MINIATURAS) && !defined(NV_ANDROID)
#define NV_MINIATURA_TV 1
#endif

#ifdef NV_MINIATURA_TV
#include "memoria.h"
#include "miniatura_motor.h"
#include "miniatura_progress.h"
#include "ajustes.h"
#include "rede.h"
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

// The engine needs FFmpeg: the .tpk 6+ and LG link it for the DTS converter
// (NV_DTS_FFMPEG). NuvioTpk40 and packages without it keep only the five
// exports at the bottom, all inert (the provider is NULL, "No aparelho" never
// appears, the milestone pill stays quiet).
#if defined(NV_DTS_FFMPEG)

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
// keeps its own after the title is switched off and frees it last.
typedef struct {
  unsigned g;
  int refs;
  Tile *tiles; unsigned char *falhas; int n;   // n slots of BASE_MS
  int passo;                 // 1 = 5 s grid, DEZ = 10 s grid, 0 = not decided yet
  int feitos;
  int completo;              // the worker went through the whole grid
  long dur;
  volatile int parar;
  int vivo;                  // the worker thread is running
} Job;

static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static int estado = SEEKR_DESLIGADO;
static unsigned geracao;
static Job *job;                 // the title being shown (NULL = none)
static char urlAtual[4096];      // its link (identity: SeekrFonte.url)
static long durAtual;
static Uint32 batida;            // last pedir: the player still shows this title
static MiniProgress progresso;   // the player pill milestones (mini_progress)

static long alvoScrub = -1; static Uint32 alvoTick;

// VIDEO STATE PUBLISHED BY THE PLAYER THREAD (NV_MINIATURA_SNAP marks this
// revision for the white-box fixture). prov_pedir copies the fields of the
// SeekrFonte the player already builds every tick; the worker reads ONLY this
// copy — it never calls video_* / ajustes_* itself: off the player thread
// some of those calls mutate shared state (video_pos() on the TPK can clear
// posReconLeg, which the reconnect logic owns) and the ajustes reads are
// unsynchronized. Guarded by mu; the worker takes it through estCopia().
typedef struct {
  long playheadMs;          // SeekrFonte.playheadMs (video_pos on the player thread)
  int pronto;               // video_pronto()
  int tocando;              // video_tocando()
  unsigned bufferandoMs;    // video_bufferando_ms()
  int dts;                  // video_dts_estado()
  int largura;              // ajustes_miniatura_largura()
} MiniEst;
static MiniEst videoEst;
#define NV_MINIATURA_SNAP 1
static MiniEst estCopia(void) {
  MiniEst e;
  pthread_mutex_lock(&mu); e = videoEst; pthread_mutex_unlock(&mu);
  return e;
}

// Caller holds mu.
static void soltarJob(Job *j) {
  if (!j || --j->refs > 0) return;
  for (int k = 0; k < j->n; k++) free(j->tiles[k].jpg);
  free(j->tiles); free(j->falhas); free(j);
}

// ------------------------------------------------------------------ helpers
// Device probes. NV_MINIATURA_MEM_KB / NV_MINIATURA_RAM_MB replace the reads
// (host tests only): the values must be arbitrary expressions the test owns.
static long memDisponivelKB(void) {
#ifdef NV_MINIATURA_MEM_KB
  return (NV_MINIATURA_MEM_KB);
#else
  FILE *f = fopen("/proc/meminfo", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (!strncmp(l, "MemAvailable:", 13)) { v = atol(l + 13); break; }
  fclose(f);
  return v;
#endif
}
static long memTotalMB(void) {
#ifdef NV_MINIATURA_RAM_MB
  return (NV_MINIATURA_RAM_MB);
#else
  FILE *f = fopen("/proc/meminfo", "r");
  char l[128]; long v = -1;
  if (!f) return -1;
  while (fgets(l, sizeof l, f)) if (!strncmp(l, "MemTotal:", 9)) { v = atol(l + 9) / 1024; break; }
  fclose(f);
  return v;
#endif
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
// The stop flag is the only job field the worker polls WITHOUT the mutex (the
// engine and the network layer take it as `volatile int *`). Keep the field a
// plain int for those pointers, but make our own accesses atomic so the stop
// edge is a real happens-before (TSan-clean) and the worker never reads a
// half-written flag.
static int jobParar(const Job *j) { return __atomic_load_n(&j->parar, __ATOMIC_ACQUIRE); }
static void dormirJ(Job *j, int ms) { while (ms > 0 && !jobParar(j)) { int p = ms < 100 ? ms : 100; usleep((useconds_t)p * 1000); ms -= p; } }

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

// The grid for this file (see the top of the file). `ramMB` < 0 = unknown:
// the ref allowed the 5 s grid there; the port treats it as the 10 s one,
// coherent with the ">= 2 GB" requirement for the dense grid above 1080p.
static int gradeDe(const MiniInfo *i, long durMs, long ramMB, const char **porque) {
  int hd = i->largura <= 2048 && i->altura <= 1152;
  if (!strcmp(i->formato, "mpegts")) { *porque = "M2TS/TS"; return DEZ; }
  if (hd) {
    if (durMs <= 90L * 60000) { *porque = "up to 1080p, 90 min or shorter"; return 1; }
    *porque = "up to 1080p, longer than 90 min"; return DEZ;
  }
  if (durMs >= 70L * 60000) { *porque = "above 1080p, 70 min or longer"; return DEZ; }
  if (ramMB < 0) { *porque = "above 1080p, RAM unknown"; return DEZ; }
  if (ramMB < RAM_5S_4K_MB) { *porque = "above 1080p, under 2 GB of RAM"; return DEZ; }
  *porque = "above 1080p, shorter than 70 min";
  return 1;
}

// What the next frame costs of MemAvailable (KB): the motor's estimate raised
// to the 4K guess scaled by picture size; once MEDIDAS tiles are measured the
// worst x MARGEM wins, never below MEDIDA_MIN_KB.
static long custoEstimadoKB(const MiniInfo *mi, const long *medidas, int nMedidas, long motorKB) {
  long precisa = motorKB;
  double px = (double)mi->largura * (double)mi->altura / (3840.0 * 2160.0);
  long g = (long)(CUSTO_4K_KB * (px > 0.05 ? px : 0.05));
  if (g > precisa) precisa = g;
  if (nMedidas >= 1) {
    long maior = 0;
    for (int i = 0; i < nMedidas && i < MEDIDAS; i++) if (medidas[i] > maior) maior = medidas[i];
    maior = maior * MARGEM_PCT / 100;
    if (maior < MEDIDA_MIN_KB) maior = MEDIDA_MIN_KB;
    precisa = maior;
  }
  return precisa;
}

typedef struct {
  Job *j;
  char url[4096], cab[2048];
  char nome[512]; uint64_t bytes;
  char imdb[24]; int t, e;
} Trabalho;

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
  for (int tent = 0; tent < 3 && !jobParar(l->j); tent++) {
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

static void *trabalhar(void *u) {
  Trabalho *w = u;
  Job *j = w->j;
  Leitor *l = calloc(1, sizeof *l);
  MiniMotor *m = NULL;
  char erro[128] = "";
  // One decoding thread: on the TV 2-3 threads were no faster while the player
  // decodes 4K (tests 22/10) and cost more memory.
  const int threads = 1;
  int seguidas = 0, feitosAqui = 0, esgotou = 0;
  unsigned long somaMs = 0;
  long medidas[MEDIDAS] = {0}; int nMedidas = 0;
  Uint32 ultimaPausaLog = 0; int pausasCaladas = 0;
  if (!l) goto fim;
  l->j = j;
  snprintf(l->url, sizeof l->url, "%s", w->url);
  snprintf(l->cab, sizeof l->cab, "%s", w->cab);

  // Let the player settle first.
  for (int k = 0; k < 30 && !jobParar(j) && !estCopia().pronto; k++) dormirJ(j, 500);
  dormirJ(j, 500);
  if (jobParar(j)) goto fim;

  // Fingerprint: two 64 KiB reads (diagnostico, identidade sem a URL).
  {
    unsigned char *cab = malloc(65536), *cau = malloc(65536);
    char fp[65] = "";
    long a = cab && cau ? lerFonte(l, 0, 65535, cab) : -1;
    long b = -1;
    if (a > 0 && l->total > 65536) b = lerFonte(l, l->total - 65536, l->total - 1, cau);
    else if (a > 0) { memcpy(cau, cab, (size_t)a); b = a; }
    if (a > 0 && b > 0 && l->total > 0)
      mini_impressao((uint64_t)l->total, cab, (size_t)a, cau, (size_t)b, fp);
    free(cab); free(cau);
    printf("[miniatura] source: %lld MB, fingerprint %s, file name %s\n", (long long)(l->total / 1048576),
           fp[0] ? "read" : "unreadable", w->nome[0] ? "known" : "unknown");
    fflush(stdout);
  }

  if (jobParar(j)) goto fim;
  if (l->total <= 0) { printf("[miniatura] stopped: source size unknown\n"); goto semPrevia; }

  m = mini_motor_abrir(lerFonte, l, l->total, erro, sizeof erro);
  if (!m) { printf("[miniatura] not made for this file: %s\n", erro); goto semPrevia; }
  {
    // "Tamanho das miniaturas" (AJ_MINIATURA_RES): 0 keeps the 320-wide motor
    // default; the bigger tiles cost more decode and memory per slot.
    MiniConfig cfg;
    mini_motor_config_padrao(&cfg);
    cfg.larguraSaida = estCopia().largura;
    mini_motor_config(m, &cfg);
  }
  {
    const MiniInfo *i = mini_motor_info(m);
    const char *porque = "";
    int ps = gradeDe(i, j->dur, memTotalMB(), &porque);
    pthread_mutex_lock(&mu);
    j->passo = ps;
    pthread_mutex_unlock(&mu);
    printf("[miniatura] %s %s %dx%d %d-bit%s, %d keyframes; %s grid (%s), %d slots\n",
           i->formato, i->codec, i->largura, i->altura, i->bits, i->hdr ? " HDR" : "", i->chaves,
           ps == 1 ? "5 s" : "10 s", porque, (j->n + ps - 1) / ps);
    fflush(stdout);
  }

  while (!jobParar(j)) {
    // Pauses: the player is not showing this title, or is buffering / opening.
    // (batida is written by pedir on the calling thread: atomic edge, the field
    // itself stays a plain Uint32.)
    if ((Sint32)(SDL_GetTicks() - __atomic_load_n(&batida, __ATOMIC_ACQUIRE)) > 6000) { mini_motor_soltar_decoder(m); dormirJ(j, 500); continue; }
    // Pauses: not ready yet, buffering/opening, or paused by the control —
    // the worker yields the decoder exactly like it does while buffering.
    MiniEst e = estCopia();
    if (!e.pronto || e.bufferandoMs > 0 || !e.tocando) { mini_motor_soltar_decoder(m); dormirJ(j, 700); continue; }
    // Memory before every frame.
    long disp = memDisponivelKB();
    long motorKB = (long)(mini_motor_memoria(m, threads) / 1024);
    long precisa = custoEstimadoKB(mini_motor_info(m), medidas, nMedidas, motorKB);
    // Decoder released (pause, buffering, low memory): the next frame pays to
    // reopen it — every measured delta was taken with it open and does not
    // cover that, so the gate falls back to the engine's cold estimate.
    if (!mini_motor_decoder_aberto(m) && precisa < motorKB) precisa = motorKB;
    // Unreadable MemAvailable blocks the frame too: never decode blind.
    if (disp < 0 || disp < PISO_KB + precisa) {
      int dts = e.dts == VIDEO_DTS_PREPARANDO || e.dts == VIDEO_DTS_CONVERTIDO;
      mini_motor_soltar_decoder(m);
      // One line when it starts, then at most one every 30 s.
      if (!ultimaPausaLog || SDL_GetTicks() - ultimaPausaLog > 30000) {
        if (disp < 0)
          printf("[miniatura] paused: free memory unreadable (needs %ld MB + floor %ld MB%s)\n",
                 precisa / 1024, PISO_KB / 1024, dts ? ", DTS converting" : "");
        else
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
    long ph = e.playheadMs;
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
    if (k < 0) { esgotou = 1; break; }
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
                 j->tiles[k] = (Tile){ c, j->tiles[irmao].n, j->tiles[irmao].pts }; j->feitos++; }
        else irmao = -1;
      }
      pthread_mutex_unlock(&mu);
      if (irmao >= 0) continue;
    }

    unsigned char *jpg = NULL; int nj = 0; MiniQuadro q;
    long antes = memDisponivelKB();
    if (mini_motor_quadro(m, alvo, tol, threads, &j->parar, &jpg, &nj, &q, erro, sizeof erro) < 0) {
      if (jobParar(j)) break;
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
      j->tiles[k] = (Tile){ jpg, nj, q.ptsMs }; jpg = NULL; j->feitos++;
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
  pthread_mutex_lock(&mu);
  if (esgotou && j->feitos > 0) j->completo = 1;
  pthread_mutex_unlock(&mu);
  goto fim;
semPrevia:
  pthread_mutex_lock(&mu);
  if (j == job && !j->feitos) estado = SEEKR_SEM_PREVIA;
  pthread_mutex_unlock(&mu);
fim:
  if (m) mini_motor_fechar(m);
  fflush(stdout);
  free(l);
  pthread_mutex_lock(&mu);
  j->vivo = 0;
  soltarJob(j);
  pthread_mutex_unlock(&mu);
  free(w);
  return NULL;
}

// ------------------------------------------------------------------ title lifecycle
static void texSoltar(void);
static void prov_desligar(void);
// Caller holds mu. Every heartbeat republishes what the worker may use
// (player thread read).
static void estadoPublicar(const SeekrFonte *f) {
  __atomic_store_n(&batida, SDL_GetTicks(), __ATOMIC_RELEASE);
  videoEst.playheadMs = f->playheadMs;
  videoEst.pronto = f->pronto; videoEst.tocando = f->tocando;
  videoEst.bufferandoMs = f->bufferandoMs; videoEst.dts = f->dts;
  videoEst.largura = f->larguraMiniatura;
}
static void prov_pedir(const SeekrFonte *f) {
  Trabalho *w;
  Job *j;
  pthread_t ft;
  if (!f || !f->url || !*f->url) return;
  pthread_mutex_lock(&mu);
  estadoPublicar(f);
  // Same title still on screen: only the heartbeat.
  if (urlAtual[0] && !strcmp(urlAtual, f->url) && labs(durAtual - f->durMs) < 3000) { pthread_mutex_unlock(&mu); return; }
  pthread_mutex_unlock(&mu);
  prov_desligar();                        // the previous title's worker ends
  pthread_mutex_lock(&mu);
  snprintf(urlAtual, sizeof urlAtual, "%s", f->url);
  durAtual = f->durMs;
  __atomic_store_n(&batida, SDL_GetTicks(), __ATOMIC_RELEASE);
  if (!strncmp(f->url, "http://127.0.0.1", 16) || strstr(f->url, ".m3u8") || f->durMs < 60000) {
    estado = SEEKR_SEM_PREVIA; pthread_mutex_unlock(&mu); return;
  }
  pthread_mutex_unlock(&mu);
  j = calloc(1, sizeof *j);
  w = calloc(1, sizeof *w);
  int n = (int)((f->durMs - 1) / BASE_MS + 1);
  if (j) { j->tiles = calloc((size_t)n, sizeof *j->tiles); j->falhas = calloc((size_t)n, 1); }
  if (!j || !w || !j->tiles || !j->falhas) {
    if (j) { free(j->tiles); free(j->falhas); }
    free(j); free(w);
    return;
  }
  j->n = n; j->dur = f->durMs; j->refs = 2; j->vivo = 1;
  w->j = j;
  snprintf(w->url, sizeof w->url, "%s", f->url);
  snprintf(w->cab, sizeof w->cab, "%s", f->cabecalhos ? f->cabecalhos : "");
  snprintf(w->nome, sizeof w->nome, "%s", f->arquivo ? f->arquivo : "");
  w->bytes = f->tamanho;
  snprintf(w->imdb, sizeof w->imdb, "%s", f->imdb ? f->imdb : "");
  w->t = f->t; w->e = f->e;
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
  printf("[miniatura] new title: %ld min\n", f->durMs / 60000);
  fflush(stdout);
  if (pthread_create(&ft, NULL, trabalhar, w) == 0) pthread_detach(ft);
  else {
    pthread_mutex_lock(&mu);
    j->vivo = 0; soltarJob(j);            // the worker's reference
    estado = SEEKR_SEM_PREVIA;
    pthread_mutex_unlock(&mu);
    free(w);
  }
}

// Player thread only, on the reload path (player.c): the webOS load revision
// drops pronto/tocando and the pedir gate closes — without this the worker
// kept the stale pronto=1/tocando=1 and decoded with the decoder open until
// the 6 s heartbeat expired. Republishes the state to the job ALREADY on
// screen while its worker is alive; a title still starts exclusively through
// prov_pedir. A finished job stays referenced to show the ready grid, but
// with the worker gone nothing consumes videoEst/batida — publishing to it
// only refreshed a dead heartbeat.
void miniatura_tv_fonte_estado(const SeekrFonte *f) {
  pthread_mutex_lock(&mu);
  if (f && f->url && job && job->vivo && urlAtual[0] && !strcmp(urlAtual, f->url))
    estadoPublicar(f);
  pthread_mutex_unlock(&mu);
}

static void prov_desligar(void) {
  // The worker keeps its own reference and frees the set last.
  pthread_mutex_lock(&mu);
  if (job) { __atomic_store_n(&job->parar, 1, __ATOMIC_RELEASE); soltarJob(job); job = NULL; }
  geracao++;
  urlAtual[0] = 0; durAtual = 0;
  estado = SEEKR_DESLIGADO;
  pthread_mutex_unlock(&mu);
  // The texture names die with the provider, not on the next title (draw
  // thread: the GL context is current wherever desligar is invoked from).
  texSoltar();
}

// ------------------------------------------------------------------ SeekrProvedor
static int provTemChave(void) { return 1; }          // nothing to configure: made on this TV
static int provEstado(void) { int r; pthread_mutex_lock(&mu); r = estado; pthread_mutex_unlock(&mu); return r; }
static void provUso(SeekrUso *u) { if (u) memset(u, 0, sizeof *u); }
static void provTentar(void) {}
static void provOcioso(void) {}
static const char *provRotulo(int est) {
  switch (est) {
    case SEEKR_BUSCANDO: return "Buscando miniaturas";
    case SEEKR_PRONTO: return "Miniaturas disponíveis";
    case SEEKR_SEM_PREVIA: return "Este título não tem miniaturas";
    default: return "Miniaturas não solicitadas";
  }
}

// Display side (draw thread only): a few textures keyed by slot.
typedef struct { GLuint tex; int slot; unsigned g; Uint32 uso; long pts; } Slot;
static Slot slots[SLOTS];
static Uint32 relogioUso;
static int quadroVelho;
static int provVelho(void) { return quadroVelho; }

// Releases every uploaded tile name. Draw thread only (the GL context must be
// current): prov_desligar runs wherever the provider is switched off.
static void texSoltar(void) {
  for (int i = 0; i < SLOTS; i++) if (slots[i].tex) {
    gfx_tex_esquecer(slots[i].tex);
    glDeleteTextures(1, &slots[i].tex);
    slots[i].tex = 0;
  }
  quadroVelho = 0;
}

static int slotTex(int k, unsigned g) {
  for (int i = 0; i < SLOTS; i++) if (slots[i].tex && slots[i].slot == k && slots[i].g == g) return i;
  return -1;
}

// Uploads tile k (JPEG -> texture), keeping the slots in `proteger`. Draw thread.
static void subir(int k, unsigned g, const int *proteger, int np) {
  unsigned char *copia = NULL; int n = 0; long pts = 0;
  pthread_mutex_lock(&mu);
  if (job && job->g == g && pronto(job, k) && (copia = malloc((size_t)job->tiles[k].n)) != NULL) {
    memcpy(copia, job->tiles[k].jpg, (size_t)job->tiles[k].n);
    n = job->tiles[k].n; pts = job->tiles[k].pts;
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
  slots[alvo].pts = pts;
}

// Nearest slot with a tile within `raio` slots of k (-1 none). Caller holds mu.
static int vizinho(const Job *j, int k, int raio) {
  for (int d = 0; d <= raio; d++) {
    if (pronto(j, k + d)) return k + d;
    if (d && pronto(j, k - d)) return k - d;
  }
  return -1;
}

static int provQuadros(double posSeg, int n, GLuint *texs, double *cueSeg) {
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
    // The label is the SHOWN tile's own time, not the target's (the player
    // prints "still loading" under what is really on screen).
    if (m >= 0) { texs[c] = slots[m].tex; if (cueSeg) cueSeg[c] = slots[m].pts / 1000.0; achou = 1; quadroVelho = 1; }
  }
  return achou;
}

static const SeekrProvedor PROVEDOR = {
  provTemChave, provEstado, provRotulo, provUso, provTentar, prov_pedir,
  prov_desligar, provQuadros, provVelho, provOcioso
};

#endif // NV_DTS_FFMPEG

const SeekrProvedor *miniatura_tv_provedor(void) {
#if defined(NV_DTS_FFMPEG)
  return mini_motor_tem_ferramentas() ? &PROVEDOR : NULL;
#else
  return NULL;
#endif
}

#if !defined(NV_DTS_FFMPEG)
void miniatura_tv_fonte_estado(const SeekrFonte *f) { (void)f; }
#endif

// The feature exists on this TV only with the real tools in the package AND
// enough memory — the same rule as the TV guide (memoria.h): forced Baixa, or
// Auto with unknown or <= 2048 MB, is "pouca" and keeps the provider off.
int miniatura_tv_capaz(int modoMemoria, long ramMB) {
#if defined(NV_DTS_FFMPEG)
  static int explicado;
  int ferramentas = mini_motor_tem_ferramentas();
  int baixa = epg_memoria_eh_baixa(modoMemoria, ramMB);
  if (!explicado && (!ferramentas || baixa)) {
    explicado = 1;
    printf("[miniatura] \"No aparelho\" hidden: %s (RAM %ld MB, memory mode %d)\n",
           !ferramentas ? "the package's FFmpeg lacks h264/hevc decoders, the mjpeg encoder or the mpegts demuxer"
                        : "little memory under the guide rule",
           ramMB, modoMemoria);
    fflush(stdout);
  }
  return ferramentas && !baixa;
#else
  (void)modoMemoria; (void)ramMB;
  return 0;
#endif
}

// The set is always made from the playing file itself: never a mapped copy.
int miniatura_tv_direto(void) { return 1; }

int miniatura_tv_status_agora(void) {
#if defined(NV_DTS_FFMPEG)
  pthread_mutex_lock(&mu);
  Job *j = job;
  int possivel = j && j->passo > 0 ? (j->n + j->passo - 1) / j->passo : 0;
  int half = possivel > 0 && j->feitos * 2 >= possivel;
  int r = mini_progress(&progresso, j ? j->g : 0, j != NULL, half, j && j->completo, 0, SDL_GetTicks());
  pthread_mutex_unlock(&mu);
  return r;
#else
  return 0;
#endif
}
int miniatura_tv_prontas_agora(void) { return miniatura_tv_status_agora() == 3; }

#else
// .wgt, desktop, Android (own provider in miniatura_android.c): inert exports.
// Some harnesses compile this file without the target macros while ajustes.c
// is built WITH NV_TPK/NV_WEBOS and then references miniatura_tv_capaz — the
// stubs keep those links working; the provider stays NULL and "No aparelho"
// never appears.
const SeekrProvedor *miniatura_tv_provedor(void) { return NULL; }
void miniatura_tv_fonte_estado(const SeekrFonte *f) { (void)f; }
int miniatura_tv_capaz(int modoMemoria, long ramMB) { (void)modoMemoria; (void)ramMB; return 0; }
int miniatura_tv_direto(void) { return 0; }
int miniatura_tv_status_agora(void) { return 0; }
int miniatura_tv_prontas_agora(void) { return 0; }
#endif // NV_MINIATURA_TV
