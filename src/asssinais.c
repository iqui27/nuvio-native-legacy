// Ver asssinais.h.
#include "asssinais.h"

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The partner's lines: open addressing on (100 ms bucket of the start, text
 * hash). 131072 slots x 16 bytes = 2 MB at most, allocated on the first line;
 * a Signs & Songs track with frame-by-frame typesetting has ~37 000 blocks
 * (Kaguya S3, owner 23/10), each frame its own line. Full past 3/4: emptied
 * (the lines near the playhead come back from the relay). */
#define AS_SLOTS   131072u
#define AS_CHEIO   (AS_SLOTS / 4u * 3u)
#define AS_BALDE_MS 100
#define AS_TOL_MS  60

typedef struct { long long ms; unsigned h; unsigned usado; } AsLinha;

/* Known stretches of the partner (owner 19/11/2026: libass's rule until we
 * know). [ini, fim): from the first Cluster of a run of Clusters the relay
 * passed in a row to the start of the newest one (that one's blocks may not
 * all be in yet). A gap of more than AS_SALTO_MS between two Clusters, or a
 * jump of the stream, starts a new run. */
#define AS_TRECHOS  32
#define AS_SALTO_MS 30000LL
typedef struct { long long ini, fim; unsigned idade; } AsTrecho;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static AsLinha *tab;
static unsigned nTab;
static unsigned geracao, versao;
static int trilha, faixa;
static unsigned long escala = 1000000UL;
static int nLinhas;
static long nConsultas, nCasadas, nDialogos, nEsperando;
static AsTrecho trechos[AS_TRECHOS];
static int nTrechos, trechoAtual = -1;
static unsigned idadeTrecho;
static const void *fluxoAtual;   /* the relay connection the current run comes from */

static void esvaziar_locked(void) {
  if (tab) memset(tab, 0, AS_SLOTS * sizeof *tab);
  nTab = 0;
  nTrechos = 0; trechoAtual = -1;   /* the lines are gone: nothing is known */
}

void asssinais_definir(unsigned g, int trilhaSinais, int faixaSinais, unsigned long escalaNs) {
  pthread_mutex_lock(&trava);
  if (g == geracao && trilha == (trilhaSinais ? 1 : 0) && faixa == (faixaSinais > 0 ? faixaSinais : 0)) {
    pthread_mutex_unlock(&trava);   // the same track going on (a resumed harvest): keep the lines read
    return;
  }
  geracao = g; trilha = trilhaSinais ? 1 : 0; faixa = faixaSinais > 0 ? faixaSinais : 0;
  escala = escalaNs ? escalaNs : 1000000UL;
  esvaziar_locked();
  versao++;
  pthread_mutex_unlock(&trava);
  if (faixa)
    printf("[asssinais] generation %u: this track is %s | Signs & Songs partner for matching: track %d (data only)\n",
           g, trilha ? "Signs & Songs (no dialogue settings on any line)" : "dialogue/full", faixa);
  else
    printf("[asssinais] generation %u: this track is %s | Signs & Songs partner for matching: none\n",
           g, trilha ? "Signs & Songs (no dialogue settings on any line)" : "dialogue/full");
  fflush(stdout);
}

void asssinais_trocar_geracao(unsigned velha, unsigned nova) {
  int mudou = 0;
  pthread_mutex_lock(&trava);
  if (velha && nova && velha != nova && geracao == velha) { geracao = nova; versao++; mudou = 1; }
  pthread_mutex_unlock(&trava);
  if (mudou) {
    printf("[asssinais] the same track reloaded: generation %u -> %u (partner lines and known stretches kept)\n", velha, nova);
    fflush(stdout);
  }
}

int asssinais_trilha_sinais(unsigned g) {
  int r;
  pthread_mutex_lock(&trava); r = trilha && g == geracao; pthread_mutex_unlock(&trava);
  return r;
}

int asssinais_ativo(unsigned g) {
  int r;
  pthread_mutex_lock(&trava); r = faixa && g == geracao; pthread_mutex_unlock(&trava);
  return r;
}

int asssinais_relay_faixa(void) {
  return __atomic_load_n(&faixa, __ATOMIC_RELAXED);
}

unsigned asssinais_versao(void) {
  return __atomic_load_n(&versao, __ATOMIC_RELAXED);
}

unsigned asssinais_hash(const char *s) {
  uint32_t h = 2166136261u;
  int espaco = 0, algo = 0;
  if (!s) return 0;
  while (*s) {
    unsigned char c = (unsigned char)*s;
    if (c == '{') { const char *f = strchr(s, '}'); if (!f) break; s = f + 1; continue; }
    if (c == '\\' && (s[1] == 'N' || s[1] == 'n' || s[1] == 'h')) { espaco = 1; s += 2; continue; }
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { espaco = 1; s++; continue; }
    if (espaco && algo) { h ^= ' '; h *= 16777619u; }
    espaco = 0; algo = 1;
    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32);
    h ^= c; h *= 16777619u;
    s++;
  }
  if (!algo) return 0;
  return h ? h : 1u;
}

static unsigned slot_de(long long balde, unsigned h) {
  uint64_t k = ((uint64_t)balde * 0x9E3779B97F4A7C15ull) ^ ((uint64_t)h * 0xC2B2AE3D27D4EB4Full);
  return (unsigned)(k >> 40) & (AS_SLOTS - 1u);
}

static void inserir_locked(long long ms, unsigned h) {
  unsigned i;
  if (!tab) {
    tab = calloc(AS_SLOTS, sizeof *tab);
    if (!tab) return;
  }
  if (nTab >= AS_CHEIO) esvaziar_locked();
  i = slot_de(ms / AS_BALDE_MS, h);
  for (;;) {
    if (!tab[i].usado) { tab[i].usado = 1; tab[i].ms = ms; tab[i].h = h; nTab++; nLinhas++; versao++; return; }
    if (tab[i].h == h && tab[i].ms == ms) return;   // the same line again (relay re-read after a seek)
    i = (i + 1u) & (AS_SLOTS - 1u);
  }
}

static int achar_locked(long long ms, unsigned h) {
  long long b0 = ms / AS_BALDE_MS, b;
  if (!tab || !nTab) return 0;
  for (b = b0 - 1; b <= b0 + 1; b++) {
    unsigned i = slot_de(b, h);
    while (tab[i].usado) {
      if (tab[i].h == h && tab[i].ms / AS_BALDE_MS == b && llabs(tab[i].ms - ms) <= AS_TOL_MS) return 1;
      i = (i + 1u) & (AS_SLOTS - 1u);
    }
  }
  return 0;
}

int asssinais_casa(unsigned g, long long inicioMs, unsigned hash) {
  int r = 0;
  if (!hash) return 0;
  pthread_mutex_lock(&trava);
  if (faixa && g == geracao) {
    nConsultas++;
    r = achar_locked(inicioMs, hash);
    if (r) nCasadas++;
  }
  pthread_mutex_unlock(&trava);
  return r;
}

static int conhecido_locked(long long ms) {
  int i;
  for (i = 0; i < nTrechos; i++) if (ms >= trechos[i].ini && ms < trechos[i].fim) return 1;
  return 0;
}

static int decidir(unsigned g, long long inicioMs, unsigned hash, int contar) {
  int r = -1;
  pthread_mutex_lock(&trava);
  if (faixa && g == geracao) {
    if (hash && achar_locked(inicioMs, hash)) r = 1;
    else if (conhecido_locked(inicioMs)) r = 0;
    if (contar) { nConsultas++; if (r > 0) nCasadas++; else if (!r) nDialogos++; else nEsperando++; }
  }
  pthread_mutex_unlock(&trava);
  return r;
}
int asssinais_decidir(unsigned g, long long inicioMs, unsigned hash) { return decidir(g, inicioMs, hash, 1); }
int asssinais_decidir_desenho(unsigned g, long long inicioMs, unsigned hash) { return decidir(g, inicioMs, hash, 0); }

/* Merges the run i into any stretch it now overlaps; returns its new index. */
static int juntar_locked(int i) {
  int j;
  for (j = 0; j < nTrechos; j++) {
    if (j == i || trechos[j].fim < trechos[i].ini || trechos[j].ini > trechos[i].fim) continue;
    if (trechos[j].ini < trechos[i].ini) trechos[i].ini = trechos[j].ini;
    if (trechos[j].fim > trechos[i].fim) trechos[i].fim = trechos[j].fim;
    trechos[j] = trechos[--nTrechos];
    if (i == nTrechos) i = j;
    j = -1;   /* start again: the merged stretch may touch another */
  }
  return i;
}

void asssinais_cluster(const void *fluxo, unsigned long clTs) {
  long long ms;
  int i;
  pthread_mutex_lock(&trava);
  if (!faixa) { pthread_mutex_unlock(&trava); return; }
  ms = (long long)clTs * (long long)escala / 1000000LL;
  if (fluxo != fluxoAtual) { fluxoAtual = fluxo; trechoAtual = -1; }   /* another connection: not a run */
  i = trechoAtual;
  if (i >= 0 && i < nTrechos && ms > trechos[i].fim && ms - trechos[i].fim <= AS_SALTO_MS) {
    trechos[i].fim = ms; trechos[i].idade = ++idadeTrecho;   /* the previous Cluster is whole now */
    trechoAtual = juntar_locked(i);
    versao++;
  } else if (!(i >= 0 && i < nTrechos && ms == trechos[i].fim)) {
    /* A new run: inside a known stretch it carries on from there. */
    int k;
    for (k = 0; k < nTrechos; k++) if (ms >= trechos[k].ini && ms <= trechos[k].fim) break;
    if (k == nTrechos) {
      if (nTrechos == AS_TRECHOS) {   /* full: the stretch used longest ago goes */
        int v = 0, m;
        for (m = 1; m < nTrechos; m++) if (trechos[m].idade < trechos[v].idade) v = m;
        trechos[v] = trechos[--nTrechos];
      }
      k = nTrechos++;
      trechos[k].ini = trechos[k].fim = ms;
    } else if (trechos[k].fim > ms) {
      /* Re-reading a known stretch: the run goes on at its end once the
       * relay gets there (a cluster past fim extends it). */
    }
    trechos[k].idade = ++idadeTrecho;
    trechoAtual = k;
  }
  pthread_mutex_unlock(&trava);
}

void asssinais_salto(void) {
  pthread_mutex_lock(&trava); trechoAtual = -1; pthread_mutex_unlock(&trava);
}

void asssinais_estatisticas(AssSinaisEst *e) {
  int i, novo = -1;
  memset(e, 0, sizeof *e);
  pthread_mutex_lock(&trava);
  e->linhas = nLinhas; e->consultas = nConsultas; e->sinais = nCasadas;
  e->dialogos = nDialogos; e->esperando = nEsperando;
  e->guardadas = (int)nTab; e->trechos = nTrechos;
  for (i = 0; i < nTrechos; i++) if (novo < 0 || trechos[i].idade > trechos[novo].idade) novo = i;
  if (novo >= 0) { e->conhecidoIni = trechos[novo].ini; e->conhecidoFim = trechos[novo].fim; }
  nLinhas = 0; nConsultas = 0; nCasadas = 0; nDialogos = 0; nEsperando = 0;
  pthread_mutex_unlock(&trava);
}

/* ---- EBML, just what a subtitle block needs ---- */
static int largura(unsigned char b) { int w = 1; while (w <= 8 && !(b & (0x80 >> (w - 1)))) w++; return w; }
static long long lerTam(const unsigned char *p, long n, int *usados) {
  int w; long long v; int i;
  if (n < 1) return -1;
  w = largura(p[0]);
  if (w > 8 || w > n) return -1;
  v = p[0] & (0xFF >> w);
  for (i = 1; i < w; i++) v = (v << 8) | p[i];
  *usados = w;
  return v;
}
static long lerId(const unsigned char *p, long n, int *usados) {
  int w; long v; int i;
  if (n < 1) return -1;
  w = largura(p[0]);
  if (w > 4 || w > n) return -1;
  v = p[0];
  for (i = 1; i < w; i++) v = (v << 8) | p[i];
  *usados = w;
  return v;
}

void asssinais_bloco(unsigned long clTs, const unsigned char *b, long n) {
  int ui = 0, us = 0, ut = 0, virgulas = 0;
  long id = lerId(b, n, &ui), k;
  long long tam, trk, ms;
  const unsigned char *bl = NULL; long blN = 0;
  unsigned long esc;
  char texto[1024]; long tn = 0;
  if (id != 0xA0 && id != 0xA3) return;
  tam = lerTam(b + ui, n - ui, &us);
  if (tam < 4 || ui + us + tam > n) return;
  if (id == 0xA3) { bl = b + ui + us; blN = (long)tam; }
  else {
    const unsigned char *d = b + ui + us; long dn = (long)tam, o = 0;
    while (o < dn) {
      int a = 0, c = 0; long cid = lerId(d + o, dn - o, &a); long long ct;
      if (cid < 0) break;
      ct = lerTam(d + o + a, dn - o - a, &c);
      if (ct < 0 || o + a + c + ct > dn) break;
      if (cid == 0xA1) { bl = d + o + a + c; blN = (long)ct; break; }
      o += a + c + (long)ct;
    }
  }
  if (!bl || blN < 4) return;
  trk = lerTam(bl, blN, &ut);
  if (trk != asssinais_relay_faixa() || ut + 3 > blN) return;
  if (bl[ut + 2] & 0x06) return;   // laced: never a subtitle
  pthread_mutex_lock(&trava); esc = escala; pthread_mutex_unlock(&trava);
  ms = ((long long)clTs + (short)((bl[ut] << 8) | bl[ut + 1])) * (long long)esc / 1000000LL;
  /* "ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text" */
  for (k = ut + 3; k < blN && virgulas < 8; k++) if (bl[k] == ',') virgulas++;
  if (virgulas < 8) return;
  for (; k < blN && tn < (long)sizeof texto - 1; k++) texto[tn++] = (char)bl[k];
  texto[tn] = 0;
  { unsigned h = asssinais_hash(texto);
    if (!h) return;
    pthread_mutex_lock(&trava);
    if (faixa) inserir_locked(ms, h);
    pthread_mutex_unlock(&trava); }
}

int asssinais_tags(const char *s) {
  for (; s && *s; s++) {
    if (*s != '{') continue;
    for (s++; *s && *s != '}'; s++) {
      if (*s != '\\') continue;
      if (!strncmp(s + 1, "pos", 3) || !strncmp(s + 1, "move", 4) || !strncmp(s + 1, "clip", 4) ||
          !strncmp(s + 1, "iclip", 5) || !strncmp(s + 1, "org", 3) || s[1] == 'p')
        return 1;
    }
    if (!*s) break;
  }
  return 0;
}

int asssinais_nome(const char *nome) {
  char b[128]; size_t i, n;
  if (!nome) return 0;
  n = strlen(nome); if (n >= sizeof b) n = sizeof b - 1;
  for (i = 0; i < n; i++) { char c = nome[i]; b[i] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c); }
  b[n] = 0;
  if (strstr(b, "full") || strstr(b, "dialog")) return 0;
  return strstr(b, "sign") || strstr(b, "song") || strstr(b, "s&s") || strstr(b, "typeset") ||
         strstr(b, "forced") || strstr(b, "letreiro");
}
