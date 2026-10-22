// Pausa a identidade no ponto em que a renovacao pode esperar rede.
// Condvar, sem sleeps: a revogacao/troca sempre termina antes do POST.
#include "atividade.h"
#include "recomenda.h"
#include "plugins.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int entrou, liberar, posts, perfil = 1;
static unsigned geracao = 1;
static char disco[8000];

int perfis_ativo(void) { return perfil; }
unsigned plugins_geracao(void) { return geracao; }
int recomenda_ativo(void) { return 1; }
int recomenda_n(void) { return 0; }
int recomenda_item(int i, RecItem *r) { (void)i; (void)r; return 0; }
void recresp_marcar_assistida(long long r) { (void)r; }
void sessao_token_recusado(const char *t) { (void)t; }
char *dados_ler(const char *n) { (void)n; return disco[0] ? strdup(disco) : NULL; }
int dados_gravar(const char *n, const char *s) { (void)n; snprintf(disco, sizeof disco, "%s", s); return 1; }
int dados_apagar(const char *n) { (void)n; disco[0] = 0; return 1; }

int recomenda_cabecalhos(const char **cab, char *aut, size_t na, char *via, size_t nv,
                         char *p, size_t np) {
  pthread_mutex_lock(&trava);
  entrou = 1;
  pthread_cond_broadcast(&cond);
  while (!liberar) pthread_cond_wait(&cond, &trava);
  pthread_mutex_unlock(&trava);
  snprintf(aut, na, "Authorization: Bearer token");
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  snprintf(p, np, "X-Nuvio-Perfil: %d", perfil);
  cab[0] = aut; cab[1] = via; cab[2] = p; cab[3] = NULL;
  return 1;
}
char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *st) {
  (void)url; (void)segundos;
  printf("POST permitido=%d %s corpo=%s\n", atividade_permitido(), cab[2], corpo);
  posts++;
  *st = 200;
  return strdup("{}");
}

// Inclui o modulo real para chamar e aguardar o fio, sem thread detached.
#define ATIV_SEM_FIO
#include "../src/atividade.c"
static void *enviar(void *u) { fioEnviar(u); return NULL; }

int main(int argc, char **argv) {
  pthread_t fio;
  const char *caso = argc > 1 ? argv[1] : "";
  int esperado = !strcmp(caso, "normal");
  atividade_iniciar();
  atividade_definir_permitido(1);
  atividade_reacao("tt123", "movie", "perfil 1", "", 1, 0);
  pthread_create(&fio, NULL, enviar, NULL);
  pthread_mutex_lock(&trava);
  while (!entrou) pthread_cond_wait(&cond, &trava);
  pthread_mutex_unlock(&trava);
  if (!strcmp(caso, "revogar")) atividade_definir_permitido(0);
  else if (!strcmp(caso, "perfil")) perfil = 2;
  else if (!strcmp(caso, "conta")) geracao++;
  else if (!strcmp(caso, "recarregar")) atividade_iniciar();
  else if (!strcmp(caso, "revogar-repor")) {
    atividade_definir_permitido(0);
    atividade_definir_permitido(1);
    atividade_reacao("tt123", "movie", "perfil 1", "", 1, 0);
  } else if (!esperado) return 2;
  pthread_mutex_lock(&trava);
  liberar = 1;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&trava);
  pthread_join(fio, NULL);
  printf("%s: posts=%d (esperado %d), fila=%d\n", caso, posts, esperado, atividade_fila_n());
  return posts != esperado || atividade_fila_n() !=
         ((!strcmp(caso, "revogar") || esperado) ? 0 : 1);
}
