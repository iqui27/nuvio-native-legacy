// O VINCULO DO TRAKT E POR PERFIL.
//
// Relato do dono (LG C9, conta com 2 perfis): "o perfil 2 mostra as mesmas
// fileiras, o Trakt, tudo do perfil 1". O log da troca dizia "[trakt] vinculo
// desta TV mantido": o token vivia num trakt.txt por APARELHO, e o perfil 2 —
// que nao tem credencial Trakt na conta — seguia com o do 1.
//
// Aqui: perfil 1 vinculado e 2 nao; trocar para o 2 nao deixa token nem estado;
// voltar para o 1 restaura; autorizar no 2 grava so o arquivo do 2; o trakt.txt
// antigo vira o do perfil 1 e de mais ninguem; uma renovacao que volta depois
// da troca vai para o arquivo do perfil que a pediu; sair apaga todos.
//
// Sem SDL, sem rede e sem disco: o "disco" e uma tabela em memoria e a rede
// devolve respostas fixas. Os tokens sao de mentira.
#include "traktauth.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ---- disco em memoria ---------------------------------------------------------

#define ARQ_MAX 32
static struct { char nome[48]; char *conteudo; } arqs[ARQ_MAX];
static pthread_mutex_t travaArq = PTHREAD_MUTEX_INITIALIZER;

static int achar(const char *nome) {
  int i;
  for (i = 0; i < ARQ_MAX; i++) if (arqs[i].conteudo && !strcmp(arqs[i].nome, nome)) return i;
  return -1;
}
char *dados_ler(const char *nome) {
  char *r = NULL;
  int i;
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
static int existe(const char *nome) {
  char *b = dados_ler(nome);
  free(b);
  return b != NULL;
}
static int contem(const char *nome, const char *trecho) {
  char *b = dados_ler(nome);
  int sim = b && strstr(b, trecho);
  free(b);
  return sim;
}

// ---- trakt.c e o resto --------------------------------------------------------

static char definido[300];
static int  ativo, nEsquecer, nDefinir;
int trakt_definir(const char *tk, const char *cli) {
  (void)cli;
  snprintf(definido, sizeof definido, "%s", tk ? tk : "");
  ativo = 1;
  nDefinir++;
  return 1;
}
void trakt_esquecer(void) { definido[0] = 0; ativo = 0; nEsquecer++; }
int  trakt_ativo(void)    { return ativo; }
int  trakt_recusada(void) { return 0; }

const char *nuvem_trakt_cliente(void) { return "cliente-teste"; }
const char *nuvem_trakt_segredo(void) { return "segredo-teste"; }
int  sessao_logada(void)  { return 0; }
int  sync_estado(void)    { return 2; }
int  sync_empurrar_credencial(const char *p, const char *j) { (void)p; (void)j; return 0; }
static int nRepetir;
void desc_repetir(void)   { nRepetir++; }
const char *i18n(const char *s) { return s; }

// A rede: codigo de dispositivo, token do codigo e renovacao. `segurar` prende
// a resposta ate o teste soltar — e assim que a troca de perfil acontece com o
// fio no ar.
static volatile int segurar, noAr;
static char tokenDoCodigo[64] = "FALSO-P2";
char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  char *r = malloc(512);
  (void)segundos; (void)cab; (void)corpo;
  noAr = 1;
  while (segurar) usleep(1000);
  if (strstr(url, "/oauth/device/code")) {
    snprintf(r, 512, "{\"device_code\":\"DC-FALSO\",\"user_code\":\"ABCD1234\","
                     "\"verification_url\":\"https://trakt.tv/activate\","
                     "\"expires_in\":600,\"interval\":1}");
    *status = 200;
  } else if (strstr(url, "/oauth/device/token")) {
    snprintf(r, 512, "{\"access_token\":\"%s\",\"refresh_token\":\"R-%s\","
                     "\"created_at\":%ld,\"expires_in\":7776000}",
             tokenDoCodigo, tokenDoCodigo, (long)time(NULL));
    *status = 200;
  } else if (strstr(url, "/oauth/token")) {
    snprintf(r, 512, "{\"access_token\":\"RENOVADO-P1\",\"refresh_token\":\"R-NOVO\","
                     "\"created_at\":%ld,\"expires_in\":7776000}", (long)time(NULL));
    *status = 200;
  } else {
    snprintf(r, 512, "{}");
    *status = 404;
  }
  noAr = 0;
  return r;
}

// ---- utilidades ---------------------------------------------------------------

static unsigned relogio = 1000;
static int rodaAte(int (*cond)(void), int ms) {
  int t;
  for (t = 0; t < ms; t += 5) {
    relogio += 5;
    traktauth_passo(relogio);
    if (cond()) return 1;
    usleep(5000);
  }
  return cond();
}
static int aguardando(void) { return traktauth_estado() == TRA_AGUARDANDO; }
static int ligadoP2(void)   { return !strcmp(definido, "FALSO-P2"); }
static int p1Renovado(void) { return contem("trakt-p1.txt", "RENOVADO-P1"); }

static void linhaToken(char *dst, size_t n, const char *tk, long criado, long expira) {
  snprintf(dst, n, "%s\tcliente-teste\tR-%s\t%ld\t%ld\t0\n", tk, tk, criado, expira);
}

int main(void) {
  char linha[256];
  long agora = (long)time(NULL);

  // 1. MIGRACAO: o trakt.txt antigo e do perfil 1, e so dele. O app abre no
  //    perfil 2 (gravado de ontem): o arquivo antigo vira trakt-p1.txt, o 2
  //    fica SEM vinculo e nenhum trakt-p2.txt aparece.
  traktauth_esquecer();
  linhaToken(linha, sizeof linha, "FALSO-P1", agora, 7776000);
  dados_gravar("trakt.txt", linha);
  assert(traktauth_carregar_perfil(2) == 0);
  assert(!existe("trakt.txt"));
  assert(contem("trakt-p1.txt", "FALSO-P1"));
  assert(!existe("trakt-p2.txt"));
  assert(!ativo && !definido[0] && nDefinir == 0);
  assert(traktauth_estado() == TRA_PARADO);
  assert(traktauth_perfil() == 2);
  printf("ok  trakt.txt antigo migra para o perfil 1 e o 2 nao herda\n");

  // 2. VOLTAR AO PERFIL 1 restaura o vinculo dele.
  assert(traktauth_trocar_perfil(1) == 1);
  assert(ativo && !strcmp(definido, "FALSO-P1"));
  assert(traktauth_estado() == TRA_LIGADO);
  // Trocar para o MESMO perfil nao mexe em nada.
  nEsquecer = 0;
  assert(traktauth_trocar_perfil(1) == 0);
  assert(nEsquecer == 0 && ativo);
  printf("ok  perfil 1 volta com o vinculo dele; mesmo perfil e no-op\n");

  // 3. PERFIL 1 LIGADO -> PERFIL 2: nada do 1 fica. Nem o token em trakt.c
  //    (trakt_esquecer), nem o estado, nem codigo pendente.
  nEsquecer = 0;
  assert(traktauth_trocar_perfil(2) == 1);   // estava ligado: home remonta
  assert(nEsquecer == 1);
  assert(!ativo && !definido[0]);
  assert(traktauth_estado() == TRA_PARADO);
  assert(!traktauth_codigo()[0] && !traktauth_erro()[0]);
  // A credencial da CONTA aplicada por cima (sync.c chama trakt_definir) sai
  // na proxima troca do mesmo jeito: trocar_perfil esquece o que estiver em uso.
  trakt_definir("DA-CONTA-P2", "cliente-teste");
  assert(traktauth_trocar_perfil(1) == 1);
  assert(!strcmp(definido, "FALSO-P1"));
  assert(traktauth_trocar_perfil(2) == 1);
  assert(!ativo);
  printf("ok  troca para o perfil 2 nao deixa token nem estado do 1\n");

  // 4. AUTORIZAR NO PERFIL 2 grava so o arquivo do 2.
  traktauth_comecar();
  assert(rodaAte(aguardando, 3000));
  assert(!strcmp(traktauth_codigo(), "ABCD1234"));
  assert(existe("trakt-fluxo-p2.txt") && !existe("trakt-fluxo-p1.txt"));
  assert(rodaAte(ligadoP2, 5000));
  assert(traktauth_estado() == TRA_LIGADO);
  assert(contem("trakt-p2.txt", "FALSO-P2"));
  assert(contem("trakt-p1.txt", "FALSO-P1") && !contem("trakt-p1.txt", "FALSO-P2"));
  assert(!existe("trakt-fluxo-p2.txt"));
  assert(!existe("trakt.txt"));
  printf("ok  autorizar no perfil 2 grava trakt-p2.txt e nao toca no 1\n");

  // E cada um volta com o seu.
  assert(traktauth_trocar_perfil(1) == 1 && !strcmp(definido, "FALSO-P1"));
  assert(traktauth_trocar_perfil(2) == 1 && !strcmp(definido, "FALSO-P2"));
  printf("ok  cada perfil carrega o proprio vinculo\n");

  // 5. RENOVACAO NO AR DURANTE A TROCA. Perfil 1 com token vencido no papel:
  //    a renovacao sai no primeiro passo, fica presa na rede, e a pessoa troca
  //    para o perfil 3. A resposta e do 1: vai para trakt-p1.txt (o refresh
  //    velho morreu nela) e o 3 nao ve nada.
  linhaToken(linha, sizeof linha, "VENCIDO-P1", agora - 9000, 60);
  dados_gravar("trakt-p1.txt", linha);
  assert(traktauth_trocar_perfil(1) == 1);
  assert(!ativo);                          // vencido: nao definido, renova antes
  segurar = 1;
  relogio += 5; traktauth_passo(relogio);  // solta o fio da renovacao
  { int t; for (t = 0; t < 2000 && !noAr; t++) usleep(1000); }
  assert(noAr);
  nDefinir = 0;
  assert(traktauth_trocar_perfil(3) == 1);   // o 1 tinha vinculo: remonta
  assert(!ativo && traktauth_estado() == TRA_PARADO);
  segurar = 0;
  assert(rodaAte(p1Renovado, 3000));
  { int t; for (t = 0; t < 50; t++) { relogio += 5; traktauth_passo(relogio); usleep(2000); } }
  assert(nDefinir == 0 && !ativo);         // nada aplicado no perfil 3
  assert(!existe("trakt-p3.txt"));
  assert(traktauth_estado() == TRA_PARADO);
  assert(nRepetir == 1);                   // so a remontagem do vinculo do 2
  assert(traktauth_trocar_perfil(1) == 1 && !strcmp(definido, "RENOVADO-P1"));
  printf("ok  renovacao que volta depois da troca vai para o perfil que pediu\n");

  // 6. ARQUIVO ANTIGO COM O PERFIL 1 JA SEPARADO: o antigo esta velho e sai, sem
  //    sobrescrever o do 1.
  dados_gravar("trakt.txt", "VELHO\tcliente-teste\n");
  assert(traktauth_carregar_perfil(1) == 1);
  assert(!existe("trakt.txt"));
  assert(contem("trakt-p1.txt", "RENOVADO-P1"));
  printf("ok  trakt.txt antigo nao passa por cima do trakt-p1.txt\n");

  // 7. SAIR DA CONTA apaga o vinculo de TODOS os perfis.
  traktauth_esquecer();
  assert(!existe("trakt-p1.txt") && !existe("trakt-p2.txt") && !existe("trakt.txt"));
  assert(traktauth_estado() == TRA_PARADO && traktauth_perfil() == 1);
  printf("ok  sair apaga o vinculo de todos os perfis\n");

  printf("traktauth_perfil: tudo ok\n");
  return 0;
}
