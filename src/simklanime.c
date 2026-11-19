// See simklanime.h for why this exists and the rules.
#include "simklanime.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// File: a header line, then per entry "<saved epoch> <body length> <key>\n",
// the body and a newline. Keys never hold spaces ("simkl-anime/38636").
#define CAB "nuvio-simkl-anime 1\n"

typedef struct {
  char chave[64];
  long long quando;
  char *corpo;
  size_t n;
} Entrada;

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Entrada *ents;
static int nEnts, capEnts, carregado;

static void limpar(void) {
  for (int i = 0; i < nEnts; i++) free(ents[i].corpo);
  free(ents);
  ents = NULL;
  nEnts = capEnts = 0;
}

static int achar(const char *chave) {
  for (int i = 0; i < nEnts; i++)
    if (!strcmp(ents[i].chave, chave)) return i;
  return -1;
}

// Adds or replaces; takes `corpo`. 0 out of memory (corpo freed).
static int por(const char *chave, long long quando, char *corpo, size_t n) {
  int i = achar(chave);
  if (i < 0) {
    if (nEnts == capEnts) {
      int cap = capEnts ? capEnts * 2 : 32;
      Entrada *novo = realloc(ents, sizeof *ents * (size_t)cap);
      if (!novo) { free(corpo); return 0; }
      ents = novo;
      capEnts = cap;
    }
    i = nEnts++;
    snprintf(ents[i].chave, sizeof ents[i].chave, "%s", chave);
  } else {
    free(ents[i].corpo);
  }
  ents[i].quando = quando;
  ents[i].corpo = corpo;
  ents[i].n = n;
  return 1;
}

static size_t total(void) {
  size_t t = 0;
  for (int i = 0; i < nEnts; i++) t += ents[i].n + strlen(ents[i].chave) + 32;
  return t;
}

// Oldest out until under the cap; `fica` (just saved) stays.
static void caber(int fica) {
  while (total() > SIMKLANIME_MAX_BYTES && nEnts > 1) {
    int velho = -1;
    for (int i = 0; i < nEnts; i++)
      if (i != fica && (velho < 0 || ents[i].quando < ents[velho].quando)) velho = i;
    if (velho < 0) break;
    free(ents[velho].corpo);
    ents[velho] = ents[--nEnts];
    if (fica == nEnts) fica = velho;
  }
}

// Reads the file into memory. Anything wrong with it: start empty.
static void carregar(void) {
  char *buf, *p, *fimBuf;
  carregado = 1;
  if (!(buf = dados_ler(SIMKLANIME_ARQ))) return;
  if (strncmp(buf, CAB, strlen(CAB))) {
    printf("[simkl] anime cache: unreadable file, starting empty\n");
    free(buf);
    return;
  }
  p = buf + strlen(CAB);
  // The end is measured once and each header line is read from its own
  // copy: strlen and sscanf on the file itself both walk to its end, once
  // per entry, which is slow for a file near the cap.
  fimBuf = p + strlen(p);
  while (*p) {
    char chave[64], linha[128], *fimLinha, *corpo;
    long long quando;
    unsigned long n;
    int lidos = 0;
    size_t nLinha = 0;
    if ((fimLinha = strchr(p, '\n')) && (nLinha = (size_t)(fimLinha - p)) < sizeof linha) {
      memcpy(linha, p, nLinha);
      linha[nLinha] = 0;
    } else {
      fimLinha = NULL;
    }
    if (!fimLinha ||
        sscanf(linha, "%lld %lu %63s%n", &quando, &n, chave, &lidos) != 3 ||
        (size_t)lidos != nLinha || n > SIMKLANIME_MAX_BYTES ||
        (size_t)(fimBuf - (fimLinha + 1)) < n + 1 || fimLinha[1 + n] != '\n' ||
        !(corpo = malloc(n + 1))) {
      printf("[simkl] anime cache: damaged file, starting empty\n");
      limpar();
      break;
    }
    memcpy(corpo, fimLinha + 1, n);
    corpo[n] = 0;
    if (!por(chave, quando, corpo, n)) { limpar(); break; }
    p = fimLinha + 1 + n + 1;
  }
  free(buf);
}

static void escrever(void) {
  size_t cap = strlen(CAB) + 1, k = 0;
  char *buf;
  for (int i = 0; i < nEnts; i++) cap += ents[i].n + strlen(ents[i].chave) + 48;
  if (!(buf = malloc(cap))) return;
  k += (size_t)snprintf(buf, cap, "%s", CAB);
  for (int i = 0; i < nEnts; i++) {
    k += (size_t)snprintf(buf + k, cap - k, "%lld %lu %s\n", ents[i].quando,
                          (unsigned long)ents[i].n, ents[i].chave);
    memcpy(buf + k, ents[i].corpo, ents[i].n);
    k += ents[i].n;
    buf[k++] = '\n';
  }
  buf[k] = 0;
  dados_gravar_leve(SIMKLANIME_ARQ, buf);
  free(buf);
}

char *simklanime_ler(const char *chave, long ttlSeg) {
  char *r = NULL;
  long long agora = (long long)time(NULL);
  int i;
  if (!chave) return NULL;
  pthread_mutex_lock(&trava);
  if (!carregado) carregar();
  i = achar(chave);
  // A save "from the future" (clock moved back) is not trusted either.
  if (i >= 0 && agora - ents[i].quando <= ttlSeg && ents[i].quando <= agora + 60)
    r = strdup(ents[i].corpo);
  pthread_mutex_unlock(&trava);
  return r;
}

void simklanime_gravar(const char *chave, const char *corpo) {
  simklanime_gravar_em(chave, corpo, (long long)time(NULL));
}

void simklanime_gravar_em(const char *chave, const char *corpo, long long quando) {
  size_t n;
  char *copia;
  if (!chave || !*chave || strlen(chave) >= 64 || strchr(chave, ' ') || !corpo) return;
  n = strlen(corpo);
  if (n > SIMKLANIME_MAX_BYTES || !(copia = strdup(corpo))) return;
  pthread_mutex_lock(&trava);
  if (!carregado) carregar();
  if (por(chave, quando, copia, n)) {
    caber(achar(chave));
    escrever();
  }
  pthread_mutex_unlock(&trava);
}

void simklanime_descarregar(void) {
  pthread_mutex_lock(&trava);
  limpar();
  carregado = 0;
  pthread_mutex_unlock(&trava);
}
