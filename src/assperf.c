// Ver assperf.h.
#include "assperf.h"

#include <pthread.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef NV_TPK
#include "dados.h"
#endif

#define AP_ARQ   "assperf.txt"
#define AP_MARCA "assperf-ativo.txt"   // old crash-guard mark (GPU blur, gone 27/10): deleted at start

/* The person's options start here and live in assperf.txt. Keys of the
 * removed test rows (txtres, blurres, todos_quadros, divididos, pos_inteira,
 * hinting, caches_pequenos, pequenos_100) are ignored when read and dropped on
 * the next save (owner 23/11/2026). */
static const struct { const char *chave; int min, max, padrao; } PESSOA[AP_N] __attribute__((unused)) = {
  [AP_ESCALA] = { "escala", 0, 7, 5 }, [AP_GAMA] = { "gamma", -20, 20, 0 }, [AP_CONTRASTE] = { "contrast", -30, 20, 0 },
  [AP_LIBASS] = { "libass", 0, 1, 1 }, [AP_ASSTAM] = { "asstam", 50, 200, 100 }, [AP_ASSOPAC] = { "assopac", 20, 100, 100 },
};

static int val[AP_N] __attribute__((unused));   // read off the .tpk only by the tests
static int iniciado;
static unsigned versao;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

#if defined(NV_TPK) || defined(NV_ASSPERF_TESTE)
static int valido(int id, int v) {
  if (id == AP_ESCALA) return AP_RES_AUTO(v);
  return v >= PESSOA[id].min && v <= PESSOA[id].max;
}
#endif
#ifdef NV_TPK

static void gravar(void) {
  char b[256]; size_t o = 0;
  int i;
  for (i = 0; i < AP_N && o < sizeof b; i++)
    o += (size_t)snprintf(b + o, sizeof b - o, "%s=%d\n", PESSOA[i].chave, val[i]);
  dados_gravar(AP_ARQ, b);
}
#endif

// The file is read ONCE, under the lock; `iniciado` turns 1 (release) only
// with the values in place, so an assperf_get from another thread never reads
// half a file. The first use is always an ASS track or the Style sheet, long
// after avisos_iniciar (the crash guard needs it).
/* A saved value as read from the file (owner 22/11/2026): the fixed 100 %
 * reads as High auto, the fixed 75 % as Medium auto. A saved Custom auto
 * (AP_RES_CUSTOM_VELHO, removed 23/11/2026) is not valid: the TV's default
 * stays, as if nothing were saved. */
#if defined(NV_TPK) || defined(NV_ASSPERF_TESTE)
static int valorLido(int id, int v) {
  if (id == AP_ESCALA && v == AP_RES_100_VELHO) return AP_RES_ALTO;
  if (id == AP_ESCALA && v == AP_RES_75_VELHO) return AP_RES_MEDIO;
  return v;
}
#endif

/* Default render mode by the TV (owner 23/11/2026): Tizen 7+ High auto, 6/6.5
 * Medium auto, 4/5 Low auto; unknown Medium. Only when nothing is saved. */
static int tizenMaior;      /* __atomic: video_tpk.c, once */
static int escalaSalva;     /* the file had "escala" (under trava) */
static int escala_padrao(void) {
#ifdef NV_TPK40
  return AP_RES_BAIXO;
#else
  int t = __atomic_load_n(&tizenMaior, __ATOMIC_RELAXED);
  return t >= 7 ? AP_RES_ALTO : t == 6 ? AP_RES_MEDIO : t >= 4 ? AP_RES_BAIXO : AP_RES_MEDIO;
#endif
}
void assperf_tizen_definir(int maior) {
  __atomic_store_n(&tizenMaior, maior, __ATOMIC_RELAXED);
  /* Read before the version was known and nothing saved: take the TV's default now. */
  pthread_mutex_lock(&trava);
  if (iniciado && !escalaSalva && val[AP_ESCALA] != escala_padrao()) {
    __atomic_store_n(&val[AP_ESCALA], escala_padrao(), __ATOMIC_RELAXED);
    __atomic_add_fetch(&versao, 1, __ATOMIC_ACQ_REL);
  }
  pthread_mutex_unlock(&trava);
}

void assperf_iniciar(void) {
  if (__atomic_load_n(&iniciado, __ATOMIC_ACQUIRE)) return;
  pthread_mutex_lock(&trava);
  if (iniciado) { pthread_mutex_unlock(&trava); return; }
#ifdef NV_TPK
  {
    char *t = dados_ler(AP_ARQ);
    int i, novo[AP_N] = {0}, velho[AP_N] = {0}, velhoV[AP_N] = {0};
    for (i = 0; i < AP_N; i++) val[i] = PESSOA[i].padrao;
    val[AP_ESCALA] = escala_padrao();   /* by the TV until a saved value is read */
    if (t) {
      char *l = t, *nl;
      for (; l && *l; l = nl) {
        char *eq;
        nl = strchr(l, '\n'); if (nl) *nl++ = 0;
        eq = strchr(l, '=');
        if (!eq) continue;
        *eq = 0;
        for (i = 0; i < AP_N; i++) {
          int v = valorLido(i, atoi(eq + 1));
          if (!strcmp(l, PESSOA[i].chave) && valido(i, v)) { val[i] = v; novo[i] = 1; }
        }
        /* Files written before the wider ranges (16/11/2026): "gama" was 0 off,
         * 1..5 = -1..-5, 6..10 = +5..+1; "contraste" was 30 minus the TV's
         * number. A new key in the same file wins. */
        { int v = atoi(eq + 1);
          if (!strcmp(l, "gama") && v >= 0 && v <= 10) velho[AP_GAMA] = 1, velhoV[AP_GAMA] = v == 0 ? 0 : v <= 5 ? -v : 11 - v;
          if (!strcmp(l, "contraste") && v >= 0 && v <= 30) velho[AP_CONTRASTE] = 1, velhoV[AP_CONTRASTE] = -v; }
      }
      for (i = 0; i < AP_N; i++) if (velho[i] && !novo[i]) val[i] = velhoV[i];
      escalaSalva = novo[AP_ESCALA];
      free(t);
    }
    { char *marca = dados_ler(AP_MARCA); if (marca) { free(marca); dados_apagar(AP_MARCA); } }
  }
#endif
  __atomic_store_n(&iniciado, 1, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&trava);
}

#ifdef NV_ASSPERF_TESTE
/* Tests on the computer (no NV_TPK): set the person's options without a file. */
void assperf_teste_definir(int id, int v) {
  /* The first call starts from the defaults, like the file-less .tpk. */
  if (!__atomic_load_n(&iniciado, __ATOMIC_ACQUIRE)) { int i; for (i = 0; i < AP_N; i++) val[i] = PESSOA[i].padrao; }
  __atomic_store_n(&iniciado, 1, __ATOMIC_RELEASE);
  if (id >= 0 && id < AP_N) { val[id] = v; __atomic_add_fetch(&versao, 1, __ATOMIC_ACQ_REL); }
}
/* What a saved line "key=v" becomes (-1 = not accepted). */
int assperf_teste_lido(int id, int v) { v = valorLido(id, v); return valido(id, v) ? v : -1; }
#endif

int assperf_get(int id) {
#ifdef NV_ASSPERF_TESTE
  return id >= 0 && id < AP_N ? __atomic_load_n(&val[id], __ATOMIC_RELAXED) : 0;
#elif defined(NV_TPK)
  if (id < 0 || id >= AP_N) return 0;
  assperf_iniciar();
  return __atomic_load_n(&val[id], __ATOMIC_RELAXED);
#else
  (void)id;
  return 0;
#endif
}

unsigned assperf_versao(void) { return __atomic_load_n(&versao, __ATOMIC_ACQUIRE); }

void assperf_definir(int id, int v) {
#ifdef NV_TPK
  if (id < 0 || id >= AP_N || !valido(id, v)) return;
  assperf_iniciar();
  pthread_mutex_lock(&trava);
  __atomic_store_n(&val[id], v, __ATOMIC_RELAXED);
  gravar();
  pthread_mutex_unlock(&trava);
  __atomic_add_fetch(&versao, 1, __ATOMIC_ACQ_REL);
#else
  (void)id; (void)v;
#endif
}

void assperf_restaurar(void) {
#ifdef NV_TPK
  int i;
  assperf_iniciar();
  pthread_mutex_lock(&trava);
  for (i = 0; i < AP_N; i++) if (i != AP_LIBASS) __atomic_store_n(&val[i], PESSOA[i].padrao, __ATOMIC_RELAXED);
  __atomic_store_n(&val[AP_ESCALA], escala_padrao(), __ATOMIC_RELAXED);   /* the TV's default mode */
  gravar();
  pthread_mutex_unlock(&trava);
  __atomic_add_fetch(&versao, 1, __ATOMIC_ACQ_REL);
#endif
}

/* The TV's gamma slider (-5..+5) picks a relative curve in the GAMMATBLOSD_384
 * LUT of libave.so: out = in^(1 - 0.0426 x slider), measured on the 11 curves
 * (avoc_gamma_contrast_picture.md, 24/10). "-" = higher gamma (darker mids).
 * Only the video goes through it; the subtitles get the same sum. Black and
 * white do not move. The subtitle row goes on past the TV's ends with the same
 * step (owner 16/11/2026: -20..+20; at +20 the exponent is 0.148). */
double assperf_gama(void) {
  int n = assperf_get(AP_GAMA);
  if (n < -20 || n > 20) n = 0;
  return 1.0 - 0.0426 * n;
}
/* The TV's Contrast is a GAIN per RGB channel before the gamma curve
 * (PontusMVideoEnhancer::m_CalculateContrast in libave.so: 944 x (c + 70) /
 * 120; avoc_contrast_application.md, 24/10). Relative to 30 (where subtitle
 * white matches video white on the S95C): (c + 70) / 100. The row takes the
 * TV's whole 0..50 (owner 16/11/2026, TV at 38): above 30 the gain is over
 * 1.0, so mid-tones brighten while full white stays clipped at 255. */
double assperf_contraste(void) {
  int d = assperf_get(AP_CONTRASTE);   // the TV's number minus 30: 0 = neutral everywhere
  if (d < -30 || d > 20) d = 0;
  return (30 + d + 70) / 100.0;
}

static double agoraS(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/* ---- Busy scenes ----------------------------------------------------------
 * Picture sync (legimagem.c), render resolution Auto and the frame-by-frame
 * worker (assrender.c) follow this flag. Owner 26/10, log 12: the cost of ONE
 * new frame made the fingers scene (19:00) busy on every 40 ms frame and let
 * the phone scene (22:12) fall back to normal halfway. So it is the subtitle
 * WORK PER SECOND (libass on the worker + drawing on the GL thread) over a 2 s
 * window. Logs 9 and 12: fingers 2-66 ms/s, phone 330-624 ms/s.
 *   - busy above CG_LIGA in the window, or when the forecast says so;
 *   - normal only after CG_LEVE_S seconds of playing below CG_DESLIGA with a
 *     simple screen and no busy forecast (a pause does not count);
 *   - CG_OCIOSO_S with nothing new (the scene is over): normal. */
#define CG_BALDES   20      /* 100 ms each: 2 s */
#define CG_LIGA     300.0   /* ms of work per second */
#define CG_DESLIGA  150.0
#define CG_LEVE_S   3.0
#define CG_OCIOSO_S 8.0
/* Forecast (the next second of the track, assrender.c): busy with this many
 * lines starting or this much text. Normal only with few pictures on screen. */
#define CG_PREV_INICIOS 300
#define CG_PREV_CHARS   400000L
#define CG_TELA_SIMPLES 20
static pthread_mutex_t autoTrava = PTHREAD_MUTEX_INITIALIZER;
static struct { double ms[CG_BALDES]; long long id[CG_BALDES]; double ult, leveAcum; int on, prevPesada, prevCedo, imagens; } CG;

static double carga_locked(double t) {
  long long b = (long long)(t * 10.0);   // t >= 0 (CLOCK_MONOTONIC): the cast is the floor
  double soma = 0;
  int i;
  for (i = 0; i < CG_BALDES; i++) if (CG.id[i] <= b && b - CG.id[i] < CG_BALDES) soma += CG.ms[i];
  return soma / (CG_BALDES / 10.0);
}
static void carga_somar_locked(double t, double ms) {
  long long b = (long long)(t * 10.0);   // t >= 0 (CLOCK_MONOTONIC): the cast is the floor
  int i = (int)(b % CG_BALDES);
  double c, dt = CG.ult > 0 ? t - CG.ult : 0;
  if (CG.id[i] != b) { CG.id[i] = b; CG.ms[i] = 0; }
  CG.ms[i] += ms > 0 ? ms : 0;
  c = carga_locked(t);
  if (!CG.on) {
    if (c > CG_LIGA || CG.prevPesada) {
      CG.on = 1; CG.leveAcum = 0;
    }
  } else if (c < CG_DESLIGA && !CG.prevPesada && CG.imagens < CG_TELA_SIMPLES) {
    CG.leveAcum += dt < 0 ? 0 : dt > 0.25 ? 0.25 : dt;   /* only while playing: a pause sends no tick (assrender) */
    if (CG.leveAcum >= CG_LEVE_S) CG.on = 0;
  } else CG.leveAcum = 0;
  CG.ult = t;
}
static int pesado_locked(double t) {
  if (CG.on && CG.ult > 0 && t - CG.ult > CG_OCIOSO_S) CG.on = 0;   /* nothing new: the scene is over */
  return CG.on;
}
void assperf_previsao(int inicios, long chars, int imagens, int iniciosCedo, long charsCedo) {
  pthread_mutex_lock(&autoTrava);
  CG.imagens = imagens;
  CG.prevPesada = inicios >= CG_PREV_INICIOS || chars >= CG_PREV_CHARS;
  /* The same thresholds, 0.5 s further on: only Auto looks at it, so the
   * busy flag (picture sync) still switches with the scene itself. */
  CG.prevCedo = iniciosCedo >= CG_PREV_INICIOS || charsCedo >= CG_PREV_CHARS;
  carga_somar_locked(agoraS(), 0.0);   /* now: on before the first busy frame */
  pthread_mutex_unlock(&autoTrava);
}
void assperf_carga_gl(double ms) {
  pthread_mutex_lock(&autoTrava);
  carga_somar_locked(agoraS(), ms);
  pthread_mutex_unlock(&autoTrava);
}
void assperf_escala_custo(double libassMs) {
  pthread_mutex_lock(&autoTrava);
  carga_somar_locked(agoraS(), libassMs);
  pthread_mutex_unlock(&autoTrava);
}
int assperf_pesado(void) {
  int r;
  pthread_mutex_lock(&autoTrava);
  r = pesado_locked(agoraS());
  pthread_mutex_unlock(&autoTrava);
  return r;
}
/* ---- Render resolution Auto -----------------------------------------------
 * 100 % normally; lower while the subtitles are busy (owner 25/10: "only as a
 * fallback when subtitles get heavy"). At least 5 s reduced and 2 s at 100 %
 * between switches: each one redraws everything at the new size.
 * Owner 19/11/2026: while busy, Auto 50 % (default) draws the text at 50 % and
 * the blur at 40 %, Auto 75 % the text at 75 % and the blur at 55 %. */
#define AUTO_MIN75_S  5.0
#define AUTO_MIN100_S 2.0
static struct { int em75; double desde; } A;
/* Tizen 4-6 (owner 16-17/11/2026: the 2019 Tizen 5 TV dropped to ~11 fps at
 * 75 %, the 2021 Tizen 6 TV rendered heavy signs at 118-192 ms per frame) had
 * 50 % forced in busy scenes. Since 19/11/2026 Auto 50 % is everyone's
 * default and a chosen Auto 75 % is used as chosen. "em75" = reduced now. */
static int tvLenta;   /* __atomic: set once by video_tpk.c from the Tizen version */
void assperf_tv_lenta_definir(int sim) { __atomic_store_n(&tvLenta, sim ? 1 : 0, __ATOMIC_RELAXED); }
int assperf_tv_lenta(void) {
#ifdef NV_TPK40
  return 1;
#else
  return __atomic_load_n(&tvLenta, __ATOMIC_RELAXED);
#endif
}
static void auto_trocar_locked(int em75, double t) { A.em75 = em75; A.desde = t; }

/* ---- Clean blur (owner 22/11/2026) ----------------------------------------
 * Gradients built from thin stacked strips show horizontal lines when the
 * strip edges land unevenly on the pixel rows. Even for any strip offset when
 * screen pixels per script pixel f is 1/n; for strips on whole script pixels
 * also at whole f. Simulated (22/11): 1/4, 1/3, 2/5, 1/2, 1, 2, 3, 4. */
#define AP_LIMPO_N 8
static const double LIMPO[AP_LIMPO_N] = { 0.25, 1.0 / 3.0, 0.4, 0.5, 1.0, 2.0, 3.0, 4.0 };
static int gradeAlt = 1080, gradePry = 1080;   /* __atomic: the worker sets them */
void assperf_grade_definir(int alturaPx, int playResY) {
  if (alturaPx < 16) alturaPx = 1080;
  if (playResY <= 0) playResY = 288;   /* libass's default when the script has none */
  __atomic_store_n(&gradeAlt, alturaPx, __ATOMIC_RELAXED);
  __atomic_store_n(&gradePry, playResY, __ATOMIC_RELAXED);
}
void assperf_grade(int *alturaPx, int *playResY) {
  *alturaPx = __atomic_load_n(&gradeAlt, __ATOMIC_RELAXED);
  *playResY = __atomic_load_n(&gradePry, __ATOMIC_RELAXED);
}
/* The % (as a scale) of clean step c on the current grid. */
static double limpo_p(int c) {
  int alt, pry; assperf_grade(&alt, &pry);
  return LIMPO[c] * (double)pry / (double)alt;
}
static double blur_limpo(double alvo, double teto) {
  double melhor = -1.0, maior = -1.0;
  int c;
  for (c = 0; c < AP_LIMPO_N; c++) {
    double p = limpo_p(c);
    if (p < 0.25 - 1e-9 || p > teto + 1e-9) continue;
    if (p >= alvo - 1e-9 && (melhor < 0 || p < melhor)) melhor = p;
    if (p > maior) maior = p;
  }
  return melhor > 0 ? melhor : maior > 0 ? maior : alvo;
}
/* The two scales of mode v while reduced (busy). texto == blur: one pass. */
static void escalas_reduzidas(int v, double *texto, double *blur) {
  int alt, pry;
  assperf_grade(&alt, &pry);
  /* Scripts of 540 lines or fewer: 100 % (owner: 480p renders at 100). */
  if (pry <= 540) { *texto = 1.0; *blur = blur_limpo(1.0 + 1e-6, 1.0); return; }
  if (v == AP_RES_ALTO) { *texto = AP_ALTO_TEXTO; *blur = blur_limpo(AP_ALTO_BLUR, AP_BLUR_TETO); }
  else if (v == AP_RES_MEDIO) { *texto = AP_MEDIO_TEXTO; *blur = blur_limpo(AP_MEDIO_BLUR, AP_BLUR_TETO); }
  else {
    /* Low (owner 22/11/2026, log 1): when the text scale is clean itself (50 %
     * of a 1080p script on a 1080 or 4K UI), the blur uses it too: one libass
     * pass at 50 costs less than 50 + 40, and there's no split to composite. */
    int c;
    *texto = AP_BAIXO_TEXTO; *blur = blur_limpo(AP_BAIXO_BLUR, AP_BLUR_TETO);
    for (c = 0; c < AP_LIMPO_N; c++)
      if (fabs(limpo_p(c) - AP_BAIXO_TEXTO) < 1e-6) { *blur = AP_BAIXO_TEXTO; break; }
  }
}

void assperf_modo_texto(char *b, int n) {
  static const char *const NOMES[8] = { "?", "?", "?", "Medium auto", "?", "Low auto", "?", "High auto" };
  int v = assperf_get(AP_ESCALA), alt, pry;
  double tx = 1.0, bl = 1.0;
  assperf_grade(&alt, &pry);
  escalas_reduzidas(v, &tx, &bl);
  snprintf(b, (size_t)n, "%s (busy: text %.0f / blur %.1f; px per script px %.2f / %.2f; frame %d px, PlayResY %d)",
           v >= 0 && v < 8 ? NOMES[v] : "?", tx * 100.0, bl * 100.0,
           tx * alt / pry, bl * alt / pry, alt, pry);
}

int assperf_resolucao_quadro(double *texto, double *blur) {
  int v = assperf_get(AP_ESCALA);
  double t;
  *blur = 0.0;
  if (!AP_RES_AUTO(v)) {
    pthread_mutex_lock(&autoTrava);
    A.em75 = 0;
    pthread_mutex_unlock(&autoTrava);
    *texto = 1.0;
    return 0;
  }
  t = agoraS();
  pthread_mutex_lock(&autoTrava);
  { int pesado = pesado_locked(t) || CG.prevCedo;
    if (!A.em75 && pesado) auto_trocar_locked(1, t);
    else if (A.em75 && pesado) A.desde = t;
    else if (A.em75 && !pesado && t - A.desde >= AUTO_MIN75_S) auto_trocar_locked(0, t); }
  if (A.em75) escalas_reduzidas(v, texto, blur);
  else *texto = *blur = 1.0;
  pthread_mutex_unlock(&autoTrava);
  if (*blur == *texto) { *blur = 0.0; return 0; }
  return 1;
}

void assperf_escala_reset(void) {
  pthread_mutex_lock(&autoTrava);
  A.em75 = 0; A.desde = agoraS();
  memset(&CG, 0, sizeof CG);   /* new track: the load starts from zero */
  pthread_mutex_unlock(&autoTrava);
}

double assperf_escala(void) {
  int v = assperf_get(AP_ESCALA), em75;
  double tx, bl;
  if (!AP_RES_AUTO(v)) return 1.0;
  pthread_mutex_lock(&autoTrava); em75 = A.em75; pthread_mutex_unlock(&autoTrava);
  if (!em75) return 1.0;
  escalas_reduzidas(v, &tx, &bl);
  return tx;
}

double assperf_escala_blur(void) {
  int v = assperf_get(AP_ESCALA), em75;
  double tx, bl;
  if (!AP_RES_AUTO(v)) return 0.0;
  pthread_mutex_lock(&autoTrava); em75 = A.em75; pthread_mutex_unlock(&autoTrava);
  if (!em75) return 0.0;
  escalas_reduzidas(v, &tx, &bl);
  return bl != tx ? bl : 0.0;
}

int assperf_reduzido(void) {
  int v = assperf_get(AP_ESCALA);
  return AP_RES_AUTO(v) && (assperf_escala() < 1.0 || assperf_escala_blur() > 0.0);
}
