// FPS regression scenes. See fpscena.h.
#include "fpscena.h"
#include <SDL2/SDL.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void fps_zerar(FpsAmostra *a) { a->n = 0; a->upl = 0; }

void fps_add(FpsAmostra *a, double ms, int upl) {
  if (a->n < FPS_MAX) { a->ms[a->n++] = ms; a->upl += upl; }
}

static int cmpd(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}

double fps_percentil(const FpsAmostra *a, double p) {
  double v[FPS_MAX];
  int k;
  if (a->n <= 0) return 0.0;
  memcpy(v, a->ms, sizeof(double) * (size_t)a->n);
  qsort(v, (size_t)a->n, sizeof *v, cmpd);
  k = (int)ceil(p / 100.0 * a->n) - 1;
  return v[k < 0 ? 0 : k >= a->n ? a->n - 1 : k];
}

double fps_pior(const FpsAmostra *a) { return fps_percentil(a, 100.0); }

void fps_linha(const FpsAmostra *a, const char *cena, char *out, size_t cap) {
  snprintf(out, cap, "[fps-cena] %s n=%d p50=%.2f p95=%.2f p99=%.2f pior=%.2f ms upl=%ld",
           cena, a->n, fps_percentil(a, 50), fps_percentil(a, 95), fps_percentil(a, 99),
           fps_pior(a), a->upl);
}

int fps_base_achar(const char *texto, const char *cena, double *p95, long *upl) {
  size_t n = strlen(cena);
  const char *l = texto;
  while (l && *l) {
    const char *inicio = l;
    // FPSBASE is emitted by the runner and documented as a valid file format.
    if (!strncmp(inicio, "FPSBASE", 7) && (inicio[7] == ' ' || inicio[7] == '\t')) {
      inicio += 7;
      while (*inicio == ' ' || *inicio == '\t') inicio++;
    }
    if (*l != '#' && !strncmp(inicio, cena, n) && inicio[n] == ' ') {
      char *fim, *ufim;
      inicio += n;
      double p;
      long u;
      while (*inicio == ' ' || *inicio == '\t') inicio++;
      if (!*inicio || *inicio == '\n' || *inicio == '\r') return 0;
      errno = 0;
      p = strtod(inicio, &fim);
      if (fim == inicio || errno || !isfinite(p) || p < 0 || (*fim != ' ' && *fim != '\t')) return 0;
      inicio = fim;
      while (*inicio == ' ' || *inicio == '\t') inicio++;
      if (!*inicio || *inicio == '\n' || *inicio == '\r') return 0;
      errno = 0;
      u = strtol(inicio, &ufim, 10);
      if (ufim == inicio || errno) return 0;
      while (*ufim == ' ' || *ufim == '\t' || *ufim == '\r') ufim++;
      if (u < 0 || (*ufim && *ufim != '\n')) return 0;
      *p95 = p; *upl = u;
      return 1;
    }
    l = strchr(l, '\n');
    if (l) l++;
  }
  return 0;
}

int fps_gate(const FpsAmostra *a, const char *cena, double p95Base, long uplBase,
             double tolPct, double folgaMs, char *out, size_t cap) {
  double p95 = fps_percentil(a, 95), teto = p95Base * (1.0 + tolPct / 100.0) + folgaMs;
  double uteto = uplBase * 1.25 + 2;
  int ok = a->n > 0 && isfinite(p95Base) && p95Base >= 0 && uplBase >= 0 &&
           isfinite(tolPct) && tolPct >= 0 && isfinite(folgaMs) && folgaMs >= 0 &&
           p95 <= teto && (double)a->upl <= uteto;
  snprintf(out, cap, "[fps-gate] %s %s p95=%.2f (base %.2f, teto %.2f) upl=%ld (base %ld, teto %.0f)",
           cena, ok ? "OK" : "FALHA", p95, p95Base, teto, a->upl, uplBase, uteto);
  return ok;
}

// ---- TV runner ------------------------------------------------------------
typedef struct { int quadros, tecla, cada, medir; } Passo;   // cada: tecla a cada N quadros (alternada)
typedef struct { const char *nome; const Passo *passos; int n; } Cena;

#define MED 300
// Same sequences as tests/fluidez_perf.c: settle, then measure.
static const Passo HOME[] = {
  { 120, 0, 0, 0 }, { 120, SDLK_DOWN, 1000, 0 }, { MED, SDLK_RIGHT, 12, 1 },
};
static const Passo TITULO[] = {
  { 120, 0, 0, 0 }, { 1, SDLK_RETURN, 1, 0 }, { 180, 0, 0, 0 },
  { 1, SDLK_DOWN, 1, 0 }, { 180, 0, 0, 0 }, { MED, 0, 0, 1 },
  { 1, SDLK_AC_BACK, 1, 0 }, { 60, 0, 0, 0 }, { 1, SDLK_AC_BACK, 1, 0 }, { 120, 0, 0, 0 },
};
// Run separately during existing playback: never start/stop a stream or seek.
static const Passo OSD[] = {
  { 1, SDLK_DOWN, 1, 0 }, { 119, 0, 0, 0 }, { MED, SDLK_DOWN, 12, 1 },
};
static const Cena CENAS[] = {
  { "home-scroll", HOME, (int)(sizeof HOME / sizeof *HOME) },
  { "title-page", TITULO, (int)(sizeof TITULO / sizeof *TITULO) },
  { "player-osd", OSD, (int)(sizeof OSD / sizeof *OSD) },
};
#define NCENAS ((int)(sizeof CENAS / sizeof *CENAS))

static struct {
  int ativo, cena, passo, q;
  FpsAmostra a;
  const char *base;
  char escolha[256];
  int falhas;
  int (*estado)(const char *cena);
} S;

void fpscena_estado(int (*ok)(const char *cena)) { S.estado = ok; }

static int quer(int c) { return S.escolha[0] ? strstr(S.escolha, CENAS[c].nome) != NULL : c < 2; }

static void proxima(void) {
  while (S.cena < NCENAS && !quer(S.cena)) S.cena++;
  S.passo = 0; S.q = 0; fps_zerar(&S.a);
  if (S.cena >= NCENAS) {
    printf("[fps-cena] fim falhas=%d\n", S.falhas);
    S.ativo = 0;
  }
}

void fpscena_iniciar(const char *escolha, const char *base) {
  int (*estado)(const char *) = S.estado;
  memset(&S, 0, sizeof S);
  S.estado = estado;
  S.ativo = 1; S.base = base;
  if (escolha) snprintf(S.escolha, sizeof S.escolha, "%s", escolha);
  { int c, algum = 0;   // text with no scene name means Home + title (OSD needs playback)
    for (c = 0; c < NCENAS; c++) algum |= S.escolha[0] && strstr(S.escolha, CENAS[c].nome) != NULL;
    if (!algum) S.escolha[0] = 0; }
  proxima();
}

int fpscena_ativo(void) { return S.ativo; }
int fpscena_falhas(void) { return S.falhas; }

int fpscena_quadro(double ms, int upl) {
  const Cena *c;
  const Passo *p;
  int tecla = 0;
  if (!S.ativo) return 0;
  c = &CENAS[S.cena];
  p = &c->passos[S.passo];
  if (p->medir && (S.q == 0 || S.q + 1 >= p->quadros) && S.estado && !S.estado(c->nome)) {
    // wrong screen/focus: the numbers would belong to another scene
    printf("[fps-gate] %s FALHA tela inesperada\n", c->nome);
    S.falhas++; S.cena++; proxima();
    return 0;
  }
  if (p->medir) fps_add(&S.a, ms, upl);
  if (p->tecla && p->cada && S.q % p->cada == 0) {
    // RIGHT steps swing 3 right / 3 left from the first card, so the row scrolls
    // both ways and never reaches the left edge (that opens the side menu).
    tecla = p->tecla == SDLK_RIGHT && (S.q / p->cada / 3) % 2 ? SDLK_LEFT : p->tecla;
  }
  if (++S.q >= p->quadros) {
    char l[320];
    if (p->medir) {
      double b95; long bu;
      fps_linha(&S.a, c->nome, l, sizeof l);
      printf("%s\n", l);
      printf("FPSBASE %s %.3f %ld\n", c->nome, fps_percentil(&S.a, 95), S.a.upl);
      if (S.base) {
        if (!fps_base_achar(S.base, c->nome, &b95, &bu)) {
          S.falhas++;
          snprintf(l, sizeof l, "[fps-gate] %s FALHA baseline ausente/invalida", c->nome);
        } else if (!fps_gate(&S.a, c->nome, b95, bu, 10.0, 0.5, l, sizeof l)) S.falhas++;
        printf("%s\n", l);
      }
    }
    S.q = 0;
    if (++S.passo >= c->n) { S.cena++; proxima(); }
  }
  return tecla;
}
