// O VINCULO DO SIMKL E POR PERFIL, como o do Trakt (ver traktauth_perfil.c).
//
// simkl.txt era um por aparelho: o perfil 2 lia as listas e o "continuar" do
// Simkl do perfil 1. Aqui: o arquivo antigo vira o do perfil 1 e so dele, a
// troca nao deixa token, autorizar no 2 grava so o do 2, sair apaga todos.
// Disco e rede sao dubles em memoria; tokens de mentira.
#include "simklauth.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ARQ_MAX 32
static struct { char nome[48]; char *conteudo; } arqs[ARQ_MAX];
static pthread_mutex_t travaArq = PTHREAD_MUTEX_INITIALIZER;
static int achar(const char *nome) {
  int i;
  for (i = 0; i < ARQ_MAX; i++) if (arqs[i].conteudo && !strcmp(arqs[i].nome, nome)) return i;
  return -1;
}
char *dados_ler(const char *nome) {
  char *r = NULL; int i;
  pthread_mutex_lock(&travaArq);
  i = achar(nome);
  if (i >= 0) r = strdup(arqs[i].conteudo);
  pthread_mutex_unlock(&travaArq);
  return r;
}
int dados_gravar(const char *nome, const char *c) {
  int i;
  pthread_mutex_lock(&travaArq);
  i = achar(nome);
  if (i < 0) for (i = 0; i < ARQ_MAX && arqs[i].conteudo; i++) { }
  assert(i < ARQ_MAX);
  free(arqs[i].conteudo);
  snprintf(arqs[i].nome, sizeof arqs[i].nome, "%s", nome);
  arqs[i].conteudo = strdup(c);
  pthread_mutex_unlock(&travaArq);
  return 1;
}
int dados_apagar(const char *nome) {
  int i;
  pthread_mutex_lock(&travaArq);
  i = achar(nome);
  if (i >= 0) { free(arqs[i].conteudo); arqs[i].conteudo = NULL; }
  pthread_mutex_unlock(&travaArq);
  return i >= 0;
}
static int existe(const char *n) { char *b = dados_ler(n); free(b); return b != NULL; }
static int contem(const char *n, const char *t) {
  char *b = dados_ler(n); int s = b && strstr(b, t); free(b); return s;
}

const char *nuvem_simkl_cliente(void) { return "cliente-teste"; }
const char *nuvem_simkl_app(void)     { return "nuvio"; }
void nuvem_url_escapar(const char *v, char *d, unsigned t) { snprintf(d, t, "%s", v); }
int  sync_empurrar_credencial(const char *p, const char *j) { (void)p; (void)j; return 0; }
const char *i18n(const char *s) { return s; }
char *rede_baixar_st(const char *url, int seg, const char *const *cab, int *st) {
  (void)seg; (void)cab;
  *st = 200;
  if (strstr(url, "/oauth/pin/")) return strdup("{\"result\":\"OK\",\"access_token\":\"SIMKL-FALSO-P2\"}");
  return strdup("{\"result\":\"OK\",\"user_code\":\"PIN123\",\"verification_url\":\"https://simkl.com/pin\",\"expires_in\":900}");
}

static unsigned relogio = 1000;
static int rodaAte(int (*c)(void), int ms) {
  int t;
  for (t = 0; t < ms; t += 5) {
    relogio += 5000;   // o poll do Simkl e de 5 s; o relogio anda por ele
    simklauth_passo(relogio);
    if (c()) return 1;
    usleep(5000);
  }
  return c();
}
static int ligadoP2(void) { return contem("simkl-p2.txt", "SIMKL-FALSO-P2"); }

int main(void) {
  simklauth_esquecer();
  dados_gravar("simkl.txt", "SIMKL-FALSO-P1\n");
  assert(simklauth_carregar_perfil(2) == 0);
  assert(!existe("simkl.txt") && contem("simkl-p1.txt", "SIMKL-FALSO-P1"));
  assert(!existe("simkl-p2.txt") && !simklauth_token()[0]);
  printf("ok  simkl.txt antigo migra para o perfil 1 e o 2 nao herda\n");

  assert(simklauth_trocar_perfil(1) == 1);
  assert(!strcmp(simklauth_token(), "SIMKL-FALSO-P1") && simklauth_estado() == SMK_LIGADO);
  assert(simklauth_trocar_perfil(1) == 0);
  assert(simklauth_trocar_perfil(2) == 1);
  assert(!simklauth_token()[0] && simklauth_estado() == SMK_PARADO);
  printf("ok  troca de perfil leva e traz o vinculo de cada um\n");

  simklauth_comecar();
  assert(rodaAte(ligadoP2, 3000));
  assert(!strcmp(simklauth_token(), "SIMKL-FALSO-P2"));
  assert(contem("simkl-p1.txt", "SIMKL-FALSO-P1"));
  assert(simklauth_trocar_perfil(1) == 1 && !strcmp(simklauth_token(), "SIMKL-FALSO-P1"));
  printf("ok  autorizar no perfil 2 grava so simkl-p2.txt\n");

  simklauth_esquecer();
  assert(!existe("simkl-p1.txt") && !existe("simkl-p2.txt") && !simklauth_token()[0]);
  printf("ok  sair apaga o vinculo de todos os perfis\n");
  printf("simklauth_perfil: tudo ok\n");
  return 0;
}
