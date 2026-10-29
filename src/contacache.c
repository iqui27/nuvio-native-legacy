#include "contacache.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// Envelope: cinco linhas curtas e o corpo intacto. A versao muda se o formato
// mudar; um arquivo de versao desconhecida e ignorado, nunca interpretado.
#define CC_VERSAO "NVCONTA 1"
#define CC_PERFIS 32

static const char *const SUPERFICIES[] = { CC_ADDONS_TOGGLES, CC_ADDONS_APLICADOS, CC_ADDONS, CC_COLECOES, CC_BIBLIOTECA, CC_VISTOS };
#define CC_N_SUP ((int)(sizeof SUPERFICIES / sizeof SUPERFICIES[0]))
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned geracao;

static int nome(char *dst, size_t tam, const char *sup, int perfil) {
  int i, conhecida = 0;
  if (!dst || !tam || !sup || perfil < 1 || perfil > CC_PERFIS) return 0;
  for (i = 0; i < CC_N_SUP; i++) if (!strcmp(SUPERFICIES[i], sup)) conhecida = 1;
  if (!conhecida) return 0;
  snprintf(dst, tam, "conta-%s-p%d.json", sup, perfil);
  return 1;
}

static int usuarioValido(const char *u) {
  return u && u[0] && !strchr(u, '\n') && !strchr(u, '\r');
}

static const char *linha(const char *p, char *dst, size_t tam) {
  const char *fim;
  size_t n;
  if (!p || !dst || tam < 1) return NULL;
  fim = strchr(p, '\n');
  if (!fim) return NULL;
  n = (size_t)(fim - p);
  if (n >= tam) n = tam - 1;
  memcpy(dst, p, n);
  dst[n] = 0;
  return fim + 1;
}

unsigned contacache_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&trava);
  g = geracao;
  pthread_mutex_unlock(&trava);
  return g;
}

int contacache_gravar_geracao(const char *superficie, int perfil,
                             const char *usuario, const char *corpo,
                             unsigned esperada) {
  char arq[64];
  char *buf;
  size_t tam;
  int ok;
  if (!nome(arq, sizeof arq, superficie, perfil) || !usuarioValido(usuario) ||
      !corpo || !corpo[0])
    return 0;
  tam = strlen(corpo) + strlen(usuario) + strlen(superficie) + 128;
  buf = (char *)malloc(tam);
  if (!buf) return 0;
  snprintf(buf, tam, CC_VERSAO "\nowner %s\nprofile %d\nsurface %s\nsaved %ld\n%s",
           usuario, perfil, superficie, (long)time(NULL), corpo);
  pthread_mutex_lock(&trava);
  ok = esperada == geracao && dados_gravar(arq, buf);
  pthread_mutex_unlock(&trava);
  free(buf);
  return ok;
}

int contacache_gravar(const char *superficie, int perfil, const char *usuario,
                      const char *corpo) {
  return contacache_gravar_geracao(superficie, perfil, usuario, corpo,
                                  contacache_geracao());
}

char *contacache_ler(const char *superficie, int perfil, const char *usuario,
                     long *quando) {
  char arq[64], l[256], esperado[256];
  char *buf, *corpo;
  const char *p;
  long q;
  if (quando) *quando = 0;
  if (!nome(arq, sizeof arq, superficie, perfil) || !usuarioValido(usuario)) return NULL;
  pthread_mutex_lock(&trava);
  buf = dados_ler(arq);
  pthread_mutex_unlock(&trava);
  if (!buf) return NULL;
  p = linha(buf, l, sizeof l);
  if (!p || strcmp(l, CC_VERSAO)) goto recusa;
  p = linha(p, l, sizeof l);
  snprintf(esperado, sizeof esperado, "owner %s", usuario);
  if (!p || strcmp(l, esperado)) goto recusa;
  p = linha(p, l, sizeof l);
  snprintf(esperado, sizeof esperado, "profile %d", perfil);
  if (!p || strcmp(l, esperado)) goto recusa;
  p = linha(p, l, sizeof l);
  snprintf(esperado, sizeof esperado, "surface %s", superficie);
  if (!p || strcmp(l, esperado)) goto recusa;
  p = linha(p, l, sizeof l);
  if (!p || strncmp(l, "saved ", 6)) goto recusa;
  q = atol(l + 6);
  if (!*p) goto recusa;
  corpo = strdup(p);
  free(buf);
  if (corpo && quando) *quando = q;
  return corpo;
recusa:
  free(buf);
  return NULL;
}

void contacache_esquecer(void) {
  char arq[64];
  int i, perfil;
  pthread_mutex_lock(&trava);
  geracao++;
  for (i = 0; i < CC_N_SUP; i++)
    for (perfil = 1; perfil <= CC_PERFIS; perfil++)
      if (nome(arq, sizeof arq, SUPERFICIES[i], perfil)) dados_apagar(arq);
  pthread_mutex_unlock(&trava);
}

void contacache_data(long quando, char *dst, unsigned tam) {
  time_t t = (time_t)quando;
  struct tm tmv;
  if (!dst || !tam) return;
  if (quando <= 0) { snprintf(dst, tam, "?"); return; }
#if defined(_WIN32)
  tmv = *localtime(&t);
#else
  localtime_r(&t, &tmv);
#endif
  snprintf(dst, tam, "%02d/%02d %02d:%02d", tmv.tm_mday, tmv.tm_mon + 1,
           tmv.tm_hour, tmv.tm_min);
}

int contacache_falha_transitoria(int status) {
  return status == 0 || status == 429 || status >= 500;
}
