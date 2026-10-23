// CORPUS SINTETICO DO AUTOSYNC: mede aplicou certo / recusou / aplicou errado.
//
// Nenhum texto de legenda real: o "filme" e uma linha do tempo de falas
// gerada (cenas, falas de 0,7-7 s, pausas curtas dentro da cena e longas entre
// cenas) e cada fala recebe silabas inventadas. A referencia (faixa embutida)
// e uma leitura dessa linha do tempo com o seu proprio ruido; a legenda
// externa e OUTRA leitura (outro autor: ruido, falas que faltam, cortes
// diferentes) passada pela transformacao do caso. A verdade de cada fala da
// externa e conhecida, entao o erro residual e medido fala a fala.
//
// Compila contra a engine atual ou, com -DAS_ANTIGO, contra a autosync.c do
// v2.0.0 (so offset constante). tests/autosync_corpus.sh roda as duas e junta
// a tabela ANTES x DEPOIS.
//
// Classificacao por rodada:
//   certo   aceitou e o erro p95 <= 250 ms e o maior <= 1 s (ou recusou um
//           caso que nao devia mudar nada: "ja sincronizada")
//   recusou recusou (num caso que devia aplicar)
//   errado  aceitou com erro acima disso, ou aceitou um caso que devia recusar
// "hidden" (so a engine atual): falas de verdade que o mapa deixou sem
// legenda, perto da emenda de um corte. Nao contam como erro nem como certo;
// sao contadas a parte, por rodada somada.
//
// Depurar um caso: -DSO_CASO=<indice> -DSO_RODADA=<n> (com -DAS_DEPURAR na
// engine) e DUMP=<segundos> imprime as falas em volta daquele instante.
#include "autosync.h"
#include <math.h>
#include <stdio.h>
const char *i18n(const char *text) { return text; }
#include <stdlib.h>
#include <string.h>
#include <time.h>

char *rede_baixar_bin(const char *url, int segundos, long *n) {
  (void)url; (void)segundos; (void)n; return NULL;
}

// --- gerador ------------------------------------------------------------------
static unsigned long long semente;
static double unif(void) {
  semente = semente * 6364136223846793005ULL + 1442695040888963407ULL;
  return (double)(semente >> 11) / 9007199254740992.0;
}
static double normal(void) {
  double u = unif(), v = unif();
  if (u < 1e-12) u = 1e-12;
  return sqrt(-2 * log(u)) * cos(6.283185307179586 * v);
}
static double expo(double media) { double u = unif(); if (u < 1e-12) u = 1e-12; return -log(u) * media; }

typedef struct { double ini, fim; } Ev;
typedef struct { Ev *v; int n, cap; } Evs;
static void por(Evs *e, double a, double b) {
  if (e->n == e->cap) { e->cap = e->cap ? e->cap * 2 : 1024; e->v = realloc(e->v, (size_t)e->cap * sizeof *e->v); }
  e->v[e->n++] = (Ev){ a, b };
}

// Um filme: cenas de 8-48 falas, fala lognormal (mediana ~2,3 s), pausa curta
// exponencial dentro da cena, 8 s + exponencial(35 s) entre cenas.
static Evs filme(double duracao, double densidade) {
  Evs e = { 0 };
  double t = 20 + unif() * 60;
  while (t < duracao - 30) {
    int falas = 8 + (int)(unif() * 40 * densidade);
    for (int i = 0; i < falas && t < duracao - 10; i++) {
      double d = exp(log(2.3) + .45 * normal());
      if (d < .7) d = .7;
      if (d > 7) d = 7;
      por(&e, t, t + d);
      double g = .12 + expo(1.1);
      t += d + (g > 8 ? 8 : g);
    }
    t += 8 + expo(35 / densidade);
  }
  return e;
}

static const char *const silabas[] = { "ka", "lo", "mi", "ra", "te", "su", "no", "vi", "da", "pe", "zo", "ri",
  "fa", "gu", "be", "xi", "lu", "ma", "to", "ne", "sa", "qui", "ro", "de", "li", "po", "ve", "ja", "co", "bu" };
static void frase(char *dst, size_t tam) {
  size_t n = 0; int palavras = 3 + (int)(unif() * 7);
  dst[0] = 0;
  for (int w = 0; w < palavras && n + 12 < tam; w++) {
    int s = 1 + (int)(unif() * 3);
    if (w) dst[n++] = ' ';
    for (int k = 0; k < s; k++) {
      const char *x = silabas[(int)(unif() * 30)];
      size_t m = strlen(x); memcpy(dst + n, x, m); n += m;
    }
  }
  dst[n] = 0;
}

static LegendaDocumento *documento(const Evs *e, const char *id, unsigned flags, int sinais) {
  LegendaCue *v = calloc((size_t)(e->n ? e->n : 1), sizeof *v);
  for (int i = 0; i < e->n; i++) {
    v[i].inicio = e->v[i].ini; v[i].fim = e->v[i].fim;
    v[i].cor = -1; v[i].posX = v[i].posY = -1; v[i].ordem = i;
    if (sinais) { v[i].an = 8; }
    frase(v[i].texto, sizeof v[i].texto);
  }
  LegendaDocumentoInfo info = { .sessao = 7, .flags = flags };
  snprintf(info.idioma, sizeof info.idioma, "pt");
  snprintf(info.identidade, sizeof info.identidade, "%s", id);
  LegendaDocumento *d = legenda_documento_de_cues(v, e->n, &info);
  free(v);
  return d;
}

// --- casos --------------------------------------------------------------------
enum { APLICAR, MANTER, RECUSAR };
enum { T_OFFSET, T_FPS, T_INSERCAO, T_REMOCAO, T_CD1, T_CD2, T_ESPARSO, T_FORCED_FLAG, T_FORCED_SO,
       T_ALHEIA, T_SINCRONIZADA, T_JITTER, T_JITTER_FORTE, T_SEGMENTACAO, T_LIMPA, T_EXTRA_REF, T_EXTRA_DOC };
typedef struct { const char *nome; int tipo; double offset, escala, corte; int espera; } Caso;
static const Caso casos[] = {
  { "offset +2.5s same-src", T_LIMPA, 2.5, 1, 0, APLICAR },
  { "offset +0.3s",          T_OFFSET, .3, 1, 0, APLICAR },
  { "offset -0.3s",          T_OFFSET, -.3, 1, 0, APLICAR },
  { "offset +2.5s",          T_OFFSET, 2.5, 1, 0, APLICAR },
  { "offset -7.2s",          T_OFFSET, -7.2, 1, 0, APLICAR },
  { "offset +45s",           T_OFFSET, 45, 1, 0, APLICAR },
  { "offset -90s",           T_OFFSET, -90, 1, 0, APLICAR },
  { "offset +150s",          T_OFFSET, 150, 1, 0, APLICAR },
  { "offset -150s",          T_OFFSET, -150, 1, 0, APLICAR },
  { "fps 25->23.976",        T_FPS, 0, 25.0 / 23.976, 0, APLICAR },
  { "fps 25->23.976 +4s",    T_FPS, 4, 25.0 / 23.976, 0, APLICAR },
  { "fps 23.976->25",        T_FPS, 0, 23.976 / 25.0, 0, APLICAR },
  { "fps 23.976->25 -12s",   T_FPS, -12, 23.976 / 25.0, 0, APLICAR },
  { "fps 24->23.976",        T_FPS, 0, 24.0 / 23.976, 0, APLICAR },
  { "fps 24->23.976 +1.5s",  T_FPS, 1.5, 24.0 / 23.976, 0, APLICAR },
  { "fps 23.976->24 -3s",    T_FPS, -3, 23.976 / 24.0, 0, APLICAR },
  { "insert 10s mid",        T_INSERCAO, 0, 1, 10, APLICAR },
  { "insert 45s mid +2s",    T_INSERCAO, 2, 1, 45, APLICAR },
  { "insert 90s mid",        T_INSERCAO, 0, 1, 90, APLICAR },
  { "remove 10s mid",        T_REMOCAO, 0, 1, 10, APLICAR },
  { "remove 30s mid -1s",    T_REMOCAO, -1, 1, 30, APLICAR },
  { "remove 90s mid",        T_REMOCAO, 0, 1, 90, APLICAR },
  { "CD1 (first half) +3s",  T_CD1, 3, 1, 0, APLICAR },
  { "CD2 (second half)",     T_CD2, 0, 1, 0, APLICAR },
  { "sparse 35 cues +6s",    T_ESPARSO, 6, 1, 0, APLICAR },
  { "jitter ref 150ms +5s",  T_JITTER, 5, 1, 0, APLICAR },
  { "jitter ref 700ms +5s",  T_JITTER_FORTE, 5, 1, 0, APLICAR },
  { "split lines +3s",       T_SEGMENTACAO, 3, 1, 0, APLICAR },
  { "ref +15% extra lines",  T_EXTRA_REF, -4, 1, 0, APLICAR },
  { "ext +10% extra lines",  T_EXTRA_DOC, 8, 1, 0, APLICAR },
  { "already synced",        T_SINCRONIZADA, 0, 1, 0, MANTER },
  { "already synced +80ms",  T_SINCRONIZADA, .08, 1, 0, MANTER },
  { "ref forced (flag)",     T_FORCED_FLAG, 2, 1, 0, RECUSAR },
  { "ref forced-only lines", T_FORCED_SO, 2, 1, 0, RECUSAR },
  { "unrelated subtitle",    T_ALHEIA, 0, 1, 0, RECUSAR },
  { "unrelated + fps",       T_ALHEIA, 0, 25.0 / 23.976, 0, RECUSAR },
};
#define NCASOS ((int)(sizeof casos / sizeof *casos))

// Verdade: para cada fala da externa, o instante do video em que ela devia
// comecar (NAN = a fala nao existe neste video, nao entra na medida).
typedef struct { Evs ref, doc; double *verdade; unsigned flagsRef; int sinaisRef; } Par;

static void montar(const Caso *c, Par *p) {
  double dur = 5400 + unif() * 1800;
  Evs f = filme(dur, c->tipo == T_ESPARSO ? .25 : 1);
  if (c->tipo == T_ESPARSO) {   // filme quase mudo: so ~35 falas
    Evs s = { 0 }; double passo = f.n / 35.0;
    for (double k = 0; (int)k < f.n && s.n < 35; k += passo) por(&s, f.v[(int)k].ini, f.v[(int)k].fim);
    free(f.v); f = s;
  }
  // Insercao: o video tem uma cena que a externa nao conhece.
  // A emenda cai entre duas falas (um corte no meio de uma fala nao tem
  // tempo certo nenhum para ela).
  double meio = dur / 2, L = c->corte;
  for (int i = 0; i + 1 < f.n; i++) if (f.v[i + 1].ini >= meio) {
    meio = (fmax(f.v[i].fim, i ? f.v[i - 1].fim : 0) + f.v[i + 1].ini) / 2;
    if (meio < f.v[i].fim) meio = f.v[i].fim + .01;
    break;
  }
  memset(p, 0, sizeof *p);
  p->flagsRef = LEGENDA_DOC_COMPLETO;
  Evs video = { 0 };
  for (int i = 0; i < f.n; i++) {
    Ev e = f.v[i];
    if (c->tipo == T_INSERCAO && e.ini >= meio) { e.ini += L; e.fim += L; }
    por(&video, e.ini, e.fim);
  }
  if (c->tipo == T_INSERCAO) {   // falas da cena inserida (so no video)
    for (double t = meio + 2; t < meio + L - 3; t += 3 + unif() * 2) por(&video, t, t + 1.5 + unif());
  }
  // ordena (a cena inserida entrou no fim)
  for (int i = 1; i < video.n; i++) { Ev x = video.v[i]; int j = i - 1;
    while (j >= 0 && video.v[j].ini > x.ini) { video.v[j + 1] = video.v[j]; j--; } video.v[j + 1] = x; }
  // REFERENCIA: o video com ruido de autor (40 ms; 150 ms uniforme no caso
  // jitter; 700 ms no caso jitter forte), 3% das falas faltando e 3%
  // juntadas com a seguinte.
  for (int i = 0; i < video.n; i++) {
    Ev e = video.v[i];
    if (c->tipo == T_LIMPA) { por(&p->ref, e.ini, e.fim); continue; }   // mesma fonte: sem ruido
    if (unif() < .03) continue;
    if (i + 1 < video.n && video.v[i + 1].ini - e.fim < .5 && unif() < .03) { e.fim = video.v[i + 1].fim; i++; }
    double amp = c->tipo == T_JITTER ? .15 : c->tipo == T_JITTER_FORTE ? .70 : 0;
    double j = amp ? (unif() * 2 - 1) * amp : normal() * .04;
    double k = amp ? (unif() * 2 - 1) * amp : normal() * .04;
    e.ini += j; e.fim += k;
    if (e.fim < e.ini + .3) e.fim = e.ini + .3;
    if (c->tipo == T_FORCED_SO && unif() > .03) continue;   // so as placas/forcadas
    por(&p->ref, e.ini, e.fim);
  }
  if (c->tipo == T_FORCED_FLAG) p->flagsRef |= LEGENDA_DOC_FORCED;
  // SDH/letreiro sem marca no texto: falas que so a referencia tem.
  if (c->tipo == T_EXTRA_REF) {
    int n0 = p->ref.n;
    for (int k = 0; k < n0 * 15 / 100; k++) { double t = 30 + unif() * (dur - 60); por(&p->ref, t, t + .8 + unif() * 2); }
    for (int i = 1; i < p->ref.n; i++) { Ev x = p->ref.v[i]; int j = i - 1;
      while (j >= 0 && p->ref.v[j].ini > x.ini) { p->ref.v[j + 1] = p->ref.v[j]; j--; } p->ref.v[j + 1] = x; }
  }
  // EXTERNA: outra leitura das falas do FILME (sem a cena inserida).
  Evs base = f;
  if (c->tipo == T_ALHEIA) base = filme(dur, 1);   // outro filme
  int cap = base.n * 3 + 64;
  p->verdade = malloc((size_t)cap * sizeof *p->verdade);
  double a = c->escala, q = c->offset;
  double cd = dur / 2;
  for (int i = 0; i < base.n; i++) {
    Ev e = base.v[i];
    if (c->tipo != T_LIMPA && unif() < .03) continue;
    double tv = e.ini;   // verdade no video
    if (c->tipo == T_INSERCAO && e.ini >= meio) tv += L;
    if (c->tipo == T_CD1 && e.ini >= cd) continue;
    if (c->tipo == T_CD2 && e.ini < cd) continue;
    double ji = c->tipo == T_LIMPA ? 0 : normal() * .06;
    e.ini += ji; e.fim += c->tipo == T_LIMPA ? 0 : normal() * .12; tv += ji;   // a verdade e a do autor, nao a fala
    if (e.fim < e.ini + .4) e.fim = e.ini + .4;
    // doc = (video - q) / a  (a = escala do video sobre a externa)
    double di = (e.ini - q) / a, df = (e.fim - q) / a;
    if (c->tipo == T_CD2) { di -= cd - 30; df -= cd - 30; }
    if (c->tipo == T_REMOCAO && e.ini >= meio) { di += L; df += L; }
    if (di < .5) continue;   // legenda nunca comeca antes do zero
    if (c->tipo == T_SEGMENTACAO && df - di > 2.5 && unif() < .5) {
      double m = (di + df) / 2;
      p->verdade[p->doc.n] = tv;
      por(&p->doc, di, m - .04);
      double tv2 = tv + (m + .04 - di) * a;
      p->verdade[p->doc.n] = tv2;
      por(&p->doc, m + .04, df);
      continue;
    }
    p->verdade[p->doc.n] = c->tipo == T_ALHEIA ? NAN : tv;
    por(&p->doc, di, df);
  }
  if (c->tipo == T_REMOCAO) {   // a cena que o video nao tem: so na externa
    double t0 = (meio - q) / a;
    for (double t = t0 + 2; t < t0 + L - 3; t += 3 + unif() * 2) {
      // insere mantendo a ordem
      int k = p->doc.n; por(&p->doc, 0, 0);
      while (k > 0 && p->doc.v[k - 1].ini > t) { p->doc.v[k] = p->doc.v[k - 1]; p->verdade[k] = p->verdade[k - 1]; k--; }
      p->doc.v[k] = (Ev){ t, t + 1.6 + unif() }; p->verdade[k] = NAN;
    }
  }
  if (c->tipo == T_EXTRA_DOC) {   // letra de musica etc.: so a externa tem
    int n0 = p->doc.n;
    for (int k = 0; k < n0 / 10; k++) {
      double t = 30 + unif() * (dur - 60);
      int j = p->doc.n; por(&p->doc, 0, 0);
      while (j > 0 && p->doc.v[j - 1].ini > t) { p->doc.v[j] = p->doc.v[j - 1]; p->verdade[j] = p->verdade[j - 1]; j--; }
      p->doc.v[j] = (Ev){ t, t + .8 + unif() * 2 }; p->verdade[j] = NAN;
    }
  }
  free(video.v); if (base.v != f.v) free(base.v); free(f.v);
}

static int cmpD(const void *x, const void *y) { double a = *(const double *)x, b = *(const double *)y; return a < b ? -1 : a > b; }

typedef struct { int certo, recusou, errado, n, faltou; double *erros; int nErros, capErros; double tempo, tempoMax; } Placar;
static void erro(Placar *s, double e) {
  if (s->nErros == s->capErros) { s->capErros = s->capErros ? s->capErros * 2 : 4096; s->erros = realloc(s->erros, (size_t)s->capErros * sizeof *s->erros); }
  s->erros[s->nErros++] = e;
}

int main(int argc, char **argv) {
  int rodadas = argc > 1 ? atoi(argv[1]) : 8, verboso = argc > 2;
  Placar tot[NCASOS]; memset(tot, 0, sizeof tot);
  for (int c = 0; c < NCASOS; c++) {
    for (int r = 0; r < rodadas; r++) {
#ifdef SO_CASO
      if (c != SO_CASO || r != SO_RODADA) continue;
#endif
      semente = 0x9E3779B97F4A7C15ULL * (unsigned long long)(c * 131 + r + 1);
      Par p; montar(&casos[c], &p);
      LegendaDocumento *ref = documento(&p.ref, "embedded", p.flagsRef, p.sinaisRef);
      LegendaDocumento *doc = documento(&p.doc, "external", LEGENDA_DOC_COMPLETO, 0);
#ifdef SO_CASO
      if (getenv("DUMP")) { double x = atof(getenv("DUMP"));
        for (int i = 0; i < p.doc.n; i++) if (fabs(p.doc.v[i].ini - x) < 25) fprintf(stderr, "  doc %d %.2f-%.2f verdade %.2f\n", i, p.doc.v[i].ini, p.doc.v[i].fim, p.verdade[i]);
        for (int i = 0; i < p.ref.n; i++) if (fabs(p.ref.v[i].ini - x) < 25) fprintf(stderr, "  ref %d %.2f-%.2f\n", i, p.ref.v[i].ini, p.ref.v[i].fim); }
#endif
      AutoSyncConfig cfg = autosync_config(AUTOSYNC_QUICK);
      struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
#ifdef AS_ANTIGO
      AutoSyncResultado res = autosync_comparar(doc, ref, &cfg, NULL, NULL);
#else
      AutoSyncMapa *mapa = NULL;
      AutoSyncResultado res = autosync_alinhar(doc, ref, &cfg, NULL, NULL, &mapa);
#endif
      clock_gettime(CLOCK_MONOTONIC, &t1);
      double ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
      Placar *s = &tot[c]; s->n++; s->tempo += ms; if (ms > s->tempoMax) s->tempoMax = ms;
      int aceitou = res.estado == AUTOSYNC_ACCEPTED, nE = 0;
      double *es = malloc((size_t)(p.doc.n + 1) * sizeof *es), maior = 0, mudou = 0;
      if (aceitou) {
        for (int i = 0; i < p.doc.n; i++) {
          double tv = p.verdade[i], s0 = p.doc.v[i].ini, e;
#ifdef AS_ANTIGO
          double m = tv + res.offsetMs / 1000.0;
          double mu = fabs(res.offsetMs / 1000.0);
#else
          double m = autosync_mapa_tempo(mapa, isnan(tv) ? s0 : tv);
          double mu = fabs(autosync_mapa_tempo(mapa, s0) - s0);
          if (m < -1e8 && !isnan(tv)) { s->faltou++; continue; }   // fala escondida: conta a parte
#endif
          if (mu > mudou) mudou = mu;
          if (isnan(tv)) continue;
          e = fabs(m - s0);
#ifdef SO_CASO
          if (e > 1) fprintf(stderr, "    erro cue %d doc=%.2f verdade=%.2f mapa=%.2f\n", i, s0, tv, m);
#endif
          es[nE++] = e; if (e > maior) maior = e;
        }
      }
      qsort(es, (size_t)nE, sizeof *es, cmpD);
      double p95 = nE ? es[(int)(nE * .95)] : 0;
      int classe;   // 0 certo 1 recusou 2 errado
      if (casos[c].espera == RECUSAR) classe = aceitou ? (mudou > .25 ? 2 : 0) : 0;
      else if (!aceitou) classe = casos[c].espera == MANTER ? 0 : 1;
      else classe = p95 <= .25 && maior <= 1.0 ? 0 : 2;
      if (casos[c].espera == RECUSAR && !aceitou) classe = 0;
      if (classe == 0) s->certo++; else if (classe == 1) s->recusou++; else s->errado++;
      if (aceitou && casos[c].espera != RECUSAR) for (int i = 0; i < nE; i++) erro(s, es[i]);
      if (verboso) printf("  %-24s r%d %s %-20s off=%d conf=%.3f p95=%.0fms max=%.0fms %.1fms cues=%d/%d\n",
                          casos[c].nome, r, aceitou ? "ACC" : "rej", autosync_motivo(res.motivo), res.offsetMs,
                          res.confianca, p95 * 1000, maior * 1000, ms, p.doc.n, p.ref.n);
      free(es);
#ifndef AS_ANTIGO
      autosync_mapa_liberar(mapa);
#endif
      legenda_documento_liberar(ref); legenda_documento_liberar(doc);
      free(p.ref.v); free(p.doc.v); free(p.verdade);
    }
  }
  int errados = 0, recusados = 0;
  printf("%-24s %4s %6s %7s %6s %7s %7s %8s %8s %6s\n", "case", "runs", "right", "refused", "WRONG", "p50ms", "p95ms", "avg ms", "max ms", "hidden");
  for (int c = 0; c < NCASOS; c++) {
    Placar *s = &tot[c];
    if (!s->n) continue;
    qsort(s->erros, (size_t)s->nErros, sizeof *s->erros, cmpD);
    double p50 = s->nErros ? s->erros[s->nErros / 2] * 1000 : -1, p95 = s->nErros ? s->erros[(int)(s->nErros * .95)] * 1000 : -1;
    printf("%-24s %4d %6d %7d %6d %7.0f %7.0f %8.1f %8.1f %6d\n", casos[c].nome, s->n, s->certo, s->recusou, s->errado,
           p50, p95, s->tempo / s->n, s->tempoMax, s->faltou);
    errados += s->errado; recusados += s->recusou; free(s->erros);
  }
  printf("TOTAL wrong=%d refused=%d\n", errados, recusados);
  return 0;
}
