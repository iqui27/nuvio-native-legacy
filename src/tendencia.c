#include "tendencia.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define TD_FILEIRAS 16
#define TD_ITENS    64

typedef struct {
  char chave[192];
  char desde[12];                 // data da lista anterior
  int  n;
  char imdb[TD_ITENS][24];        // ordem de HOJE
  int  delta[TD_ITENS];
  char novo[TD_ITENS];
  int  temAnterior;
} Fila;

static Fila filas[TD_FILEIRAS];
static int  nFilas;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static void hoje(char *dst, unsigned tam) {
  time_t t = time(NULL);
  struct tm *tm = localtime(&t);
  if (tm) strftime(dst, tam, "%Y-%m-%d", tm);
  else snprintf(dst, tam, "%s", "1970-01-01");
}

// FNV-1a da chave: o nome do arquivo. A chave tem ate 192 bytes com ':' e
// '/', que nao servem de nome.
static void nomeDe(const char *chave, const char *sufixo, char *dst, unsigned tam) {
  unsigned long h = 2166136261ul;
  const unsigned char *p = (const unsigned char *)chave;
  while (*p) { h ^= *p++; h *= 16777619ul; h &= 0xffffffffUL; }
  snprintf(dst, tam, "tendencia/%08lx.%s", h, sufixo);
}

// Le "<data>\n<imdb>\n..." em (data, lista). Devolve quantos ids.
static int ler(const char *nome, char *data, unsigned tamData,
               char ids[][24], int max) {
  char *c = dados_ler(nome), *p, *q;
  int n = 0;
  data[0] = 0;
  if (!c) return -1;
  p = c;
  q = strchr(p, '\n');
  if (!q) { free(c); return -1; }
  *q = 0;
  snprintf(data, tamData, "%s", p);
  p = q + 1;
  while (n < max && (q = strchr(p, '\n'))) {
    *q = 0;
    if (p[0]) snprintf(ids[n++], 24, "%s", p);
    p = q + 1;
  }
  free(c);
  return n;
}

static void gravar(const char *nome, const char *data, char ids[][24], int n) {
  char buf[16 + TD_ITENS * 24];
  int k, at = 0;
  at += snprintf(buf + at, sizeof buf - at, "%s\n", data);
  for (k = 0; k < n && at < (int)sizeof buf - 26; k++)
    at += snprintf(buf + at, sizeof buf - at, "%s\n", ids[k]);
  dados_gravar_leve(nome, buf);
}

void tend_registrar(const CatFileira *f, const CatItem *itens) {
  char nomeHoje[64], nomeAnt[64], dia[12];
  char dHoje[12], dAnt[12];
  char lHoje[TD_ITENS][24], lAnt[TD_ITENS][24];
  int nHoje, nAnt, k, n;
  Fila fila;
  if (!f || !f->chave[0] || !itens || f->n < 1) return;
  { char dir[512];
    if (!dados_caminho(dir, sizeof dir, "tendencia")) return;
    mkdir(dir, 0755); }
  nomeDe(f->chave, "hoje", nomeHoje, sizeof nomeHoje);
  nomeDe(f->chave, "anterior", nomeAnt, sizeof nomeAnt);
  hoje(dia, sizeof dia);

  memset(&fila, 0, sizeof fila);
  snprintf(fila.chave, sizeof fila.chave, "%s", f->chave);
  n = f->n > TD_ITENS ? TD_ITENS : f->n;
  for (k = 0; k < n; k++)
    snprintf(fila.imdb[k], sizeof fila.imdb[k], "%s", itens[f->ini + k].imdb);
  fila.n = n;

  nHoje = ler(nomeHoje, dHoje, sizeof dHoje, lHoje, TD_ITENS);
  nAnt  = ler(nomeAnt,  dAnt,  sizeof dAnt,  lAnt,  TD_ITENS);
  if (nHoje >= 0 && strcmp(dHoje, dia) != 0) {
    // Dia novo: a lista de "hoje" passa a ser a anterior.
    memcpy(lAnt, lHoje, sizeof lAnt); nAnt = nHoje;
    snprintf(dAnt, sizeof dAnt, "%s", dHoje);
    gravar(nomeAnt, dAnt, lAnt, nAnt);
    nHoje = -1;
  }
  if (nHoje < 0) gravar(nomeHoje, dia, fila.imdb, n);

  if (nAnt > 0) {
    fila.temAnterior = 1;
    snprintf(fila.desde, sizeof fila.desde, "%s", dAnt);
    for (k = 0; k < n; k++) {
      int j, achou = -1;
      for (j = 0; j < nAnt; j++)
        if (!strcmp(lAnt[j], fila.imdb[k])) { achou = j; break; }
      if (achou < 0) { fila.novo[k] = 1; fila.delta[k] = 0; }
      else fila.delta[k] = achou - k;     // estava em j, agora em k: subiu j-k
    }
  }

  pthread_mutex_lock(&trava);
  for (k = 0; k < nFilas; k++) if (!strcmp(filas[k].chave, f->chave)) break;
  if (k == nFilas) {
    if (nFilas < TD_FILEIRAS) nFilas++;
    else k = 0;                          // sem vaga: reaproveita a primeira
  }
  filas[k] = fila;
  pthread_mutex_unlock(&trava);
}

static const Fila *achar(const char *chave) {
  int k;
  if (!chave) return NULL;
  for (k = 0; k < nFilas; k++) if (!strcmp(filas[k].chave, chave)) return &filas[k];
  return NULL;
}

int tend_delta(const char *chave, const char *imdb, int *delta, int *novo) {
  const Fila *f;
  int k, ok = 0;
  if (delta) *delta = 0;
  if (novo) *novo = 0;
  if (!imdb || !imdb[0]) return 0;
  pthread_mutex_lock(&trava);
  f = achar(chave);
  if (f && f->temAnterior) {
    for (k = 0; k < f->n; k++)
      if (!strcmp(f->imdb[k], imdb)) {
        if (delta) *delta = f->delta[k];
        if (novo)  *novo  = f->novo[k];
        ok = 1;
        break;
      }
  }
  pthread_mutex_unlock(&trava);
  return ok;
}

const char *tend_desde(const char *chave) {
  static char d[12];
  const Fila *f;
  d[0] = 0;
  pthread_mutex_lock(&trava);
  f = achar(chave);
  if (f && f->temAnterior) snprintf(d, sizeof d, "%s", f->desde);
  pthread_mutex_unlock(&trava);
  return d;
}
