#include "addonstats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#ifndef NV_ADDONSTATS_PURO
#include "dados.h"
#endif

typedef struct {
  char nome[48];
  int n;                                  // amostras guardadas (<= JANELA)
  unsigned ms[ADDONSTATS_JANELA];         // da mais antiga para a mais nova
  unsigned char ok[ADDONSTATS_JANELA];
} Reg;

static Reg reg[ADDONSTATS_MAX];
static int nReg;
static int sujo;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static Reg *achar(const char *nome, int criar) {
  int i;
  if (!nome || !*nome) return NULL;
  for (i = 0; i < nReg; i++) if (!strcasecmp(reg[i].nome, nome)) return &reg[i];
  if (!criar) return NULL;
  if (nReg >= ADDONSTATS_MAX) {
    // Cheio: o mais antigo sai. Ordem do vetor = ordem de criacao.
    memmove(&reg[0], &reg[1], sizeof reg[0] * (ADDONSTATS_MAX - 1));
    nReg = ADDONSTATS_MAX - 1;
  }
  memset(&reg[nReg], 0, sizeof reg[nReg]);
  snprintf(reg[nReg].nome, sizeof reg[nReg].nome, "%s", nome);
  return &reg[nReg++];
}

void addonstats_registrar(const char *nome, unsigned ms, int respondeu) {
  Reg *r;
  pthread_mutex_lock(&trava);
  r = achar(nome, 1);
  if (r) {
    if (r->n >= ADDONSTATS_JANELA) {
      memmove(r->ms, r->ms + 1, sizeof r->ms[0] * (ADDONSTATS_JANELA - 1));
      memmove(r->ok, r->ok + 1, sizeof r->ok[0] * (ADDONSTATS_JANELA - 1));
      r->n = ADDONSTATS_JANELA - 1;
    }
    r->ms[r->n] = ms > 600000u ? 600000u : ms;
    r->ok[r->n] = respondeu ? 1 : 0;
    r->n++;
    sujo = 1;
  }
  pthread_mutex_unlock(&trava);
}

static int mudoSeg(const Reg *r) {
  int k = 0, i;
  if (!r) return 0;
  for (i = r->n - 1; i >= 0 && !r->ok[i]; i--) k++;
  return k;
}

int addonstats_mudo_seguidas(const char *nome) {
  int k;
  pthread_mutex_lock(&trava);
  k = mudoSeg(achar(nome, 0));
  pthread_mutex_unlock(&trava);
  return k;
}

static int lento(const Reg *r, unsigned limiteMs) {
  int i, acima = 0;
  if (!r || r->n < 3 || limiteMs == 0) return 0;
  for (i = 0; i < r->n; i++) if (!r->ok[i] || r->ms[i] > limiteMs) acima++;
  return acima * 3 >= r->n * 2;
}

int addonstats_lento(const char *nome, unsigned limiteMs) {
  int v;
  pthread_mutex_lock(&trava);
  v = lento(achar(nome, 0), limiteMs);
  pthread_mutex_unlock(&trava);
  return v;
}

int addonstats_ignoravel(const char *nome, unsigned limiteMs) {
  Reg *r;
  int v;
  if (limiteMs == 0) return 0;
  pthread_mutex_lock(&trava);
  r = achar(nome, 0);
  v = r && (mudoSeg(r) >= ADDONSTATS_MUDO_IGNORA || lento(r, limiteMs));
  pthread_mutex_unlock(&trava);
  return v;
}

int addonstats_segunda_s(const char *nome, int padraoS) {
  int m;
  pthread_mutex_lock(&trava);
  m = mudoSeg(achar(nome, 0));
  pthread_mutex_unlock(&trava);
  if (m >= ADDONSTATS_MUDO_PULA) return 0;
  if (m >= 1 && padraoS > ADDONSTATS_SEGUNDA_MUDO_S) return ADDONSTATS_SEGUNDA_MUDO_S;
  return padraoS;
}

static int cmpU(const void *a, const void *b) {
  unsigned x = *(const unsigned *)a, y = *(const unsigned *)b;
  return x < y ? -1 : x > y;
}

unsigned addonstats_mediana(const char *nome) {
  Reg *r;
  unsigned v[ADDONSTATS_JANELA], m = 0;
  int i, k = 0;
  pthread_mutex_lock(&trava);
  r = achar(nome, 0);
  if (r) for (i = 0; i < r->n; i++) if (r->ok[i]) v[k++] = r->ms[i];
  pthread_mutex_unlock(&trava);
  if (k) { qsort(v, (size_t)k, sizeof v[0], cmpU); m = v[k / 2]; }
  return m;
}

int addonstats_serializar(char *dst, unsigned tam) {
  unsigned usado = 0;
  int i, j;
  if (!dst || tam < 2) return 0;
  dst[0] = 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nReg; i++) {
    int w;
    if (!reg[i].n) continue;
    w = snprintf(dst + usado, tam - usado, "%s\t", reg[i].nome);
    if (w < 0 || (unsigned)w >= tam - usado) break;
    usado += (unsigned)w;
    for (j = 0; j < reg[i].n; j++) {
      w = snprintf(dst + usado, tam - usado, "%s%u:%d", j ? " " : "", reg[i].ms[j], reg[i].ok[j]);
      if (w < 0 || (unsigned)w >= tam - usado) break;
      usado += (unsigned)w;
    }
    if (usado + 2 >= tam) break;
    dst[usado++] = '\n'; dst[usado] = 0;
  }
  pthread_mutex_unlock(&trava);
  return (int)usado;
}

void addonstats_zerar(void) {
  pthread_mutex_lock(&trava);
  nReg = 0; sujo = 0;
  pthread_mutex_unlock(&trava);
}

void addonstats_carregar(const char *texto) {
  const char *p = texto;
  pthread_mutex_lock(&trava);
  nReg = 0;
  while (p && *p) {
    const char *fim = strchr(p, '\n'), *tab;
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[512];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len); linha[len] = 0;
    tab = strchr(linha, '\t');
    if (tab && tab > linha) {
      Reg *r;
      char *q = (char *)tab + 1;
      *(char *)tab = 0;
      r = achar(linha, 1);
      while (r && *q && r->n < ADDONSTATS_JANELA) {
        char *e;
        unsigned long ms = strtoul(q, &e, 10);
        if (e == q || *e != ':') break;
        r->ms[r->n] = ms > 600000UL ? 600000u : (unsigned)ms;
        r->ok[r->n] = e[1] == '1';
        r->n++;
        q = e + 2;
        while (*q == ' ') q++;
      }
    }
    p = fim ? fim + 1 : NULL;
  }
  sujo = 0;
  pthread_mutex_unlock(&trava);
}

#ifndef NV_ADDONSTATS_PURO
#define ARQ "fonte-latencia.txt"
void addonstats_ler(void) {
  char *t = dados_ler(ARQ);
  if (t) { addonstats_carregar(t); free(t); }
}
void addonstats_salvar(void) {
  char *buf;
  int quer;
  pthread_mutex_lock(&trava);
  quer = sujo; sujo = 0;
  pthread_mutex_unlock(&trava);
  if (!quer) return;
  buf = malloc((size_t)ADDONSTATS_MAX * 160);
  if (!buf) return;
  addonstats_serializar(buf, (unsigned)ADDONSTATS_MAX * 160u);
  dados_gravar_leve(ARQ, buf);
  free(buf);
}
#endif
