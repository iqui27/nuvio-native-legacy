// Ver duracaook.h para o porque.
#include "duracaook.h"
#include "dados.h"
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Uma linha por titulo: "<escopo>\t<titulo>". Escopo = hash da conta + perfil
// (a conta nao vai em claro para o disco). Ordem = do mais antigo ao mais novo.
#define LINHA 256
static char linhas[DURACAOOK_MAX][LINHA];
static int n, carregado;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;

static void chave(const char *conta, int perfil, const char *titulo, char *b, size_t t) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (const unsigned char *p = (const unsigned char *)(conta ? conta : ""); *p; p++) {
    h ^= *p; h *= UINT64_C(1099511628211);
  }
  snprintf(b, t, "%016llx-p%d\t%s", (unsigned long long)h, perfil > 0 ? perfil : 0, titulo ? titulo : "");
}

static void carregarSemTrava(void) {
  char *t, *l, *ctx = NULL;
  carregado = 1; n = 0;
  t = dados_ler(DURACAOOK_ARQUIVO);
  if (!t) return;
  for (l = strtok_r(t, "\n", &ctx); l && n < DURACAOOK_MAX; l = strtok_r(NULL, "\n", &ctx))
    if (strchr(l, '\t') && strlen(l) < LINHA) snprintf(linhas[n++], LINHA, "%s", l);
  free(t);
}

static void gravarSemTrava(void) {
  size_t tam = (size_t)n * (LINHA + 1) + 1, k = 0;
  char *b = malloc(tam);
  if (!b) return;
  b[0] = 0;
  for (int i = 0; i < n; i++) k += (size_t)snprintf(b + k, tam - k, "%s\n", linhas[i]);
  dados_gravar(DURACAOOK_ARQUIVO, b);
  free(b);
}

int duracaook_tem(const char *conta, int perfil, const char *titulo) {
  char c[LINHA];
  int achou = 0;
  if (!titulo || !*titulo) return 0;
  chave(conta, perfil, titulo, c, sizeof c);
  pthread_mutex_lock(&trava);
  if (!carregado) carregarSemTrava();
  for (int i = 0; i < n && !achou; i++) achou = !strcmp(linhas[i], c);
  pthread_mutex_unlock(&trava);
  return achou;
}

void duracaook_guardar(const char *conta, int perfil, const char *titulo) {
  char c[LINHA];
  int i;
  if (!titulo || !*titulo) return;
  chave(conta, perfil, titulo, c, sizeof c);
  pthread_mutex_lock(&trava);
  if (!carregado) carregarSemTrava();
  for (i = 0; i < n; i++) if (!strcmp(linhas[i], c)) break;
  if (i < n) {   // ja estava: vai para o fim (o mais novo)
    memmove(linhas[i], linhas[i + 1], (size_t)(n - i - 1) * LINHA);
    n--;
  } else if (n == DURACAOOK_MAX) {
    memmove(linhas[0], linhas[1], (size_t)(n - 1) * LINHA);
    n--;
  }
  snprintf(linhas[n++], LINHA, "%s", c);
  gravarSemTrava();
  pthread_mutex_unlock(&trava);
}

void duracaook_zerar(void) {
  pthread_mutex_lock(&trava);
  n = 0; carregado = 0;
  pthread_mutex_unlock(&trava);
}
