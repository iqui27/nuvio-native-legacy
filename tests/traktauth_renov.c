// traktauth_carregar com access token vencido no papel.
//
// O relato: "o Trakt demora para aparecer". A cadeia inteira do atraso era:
// token morto definido no arranque -> primeiro ciclo de descoberta sai pedindo
// com ele -> cada chamada espera o 401 voltar -> a recusa marca a credencial ->
// a renovacao so disparava depois de 60 s de uptime (ultimaRenovacao nascia 0
// e a guarda lia isso como "tentei no instante zero") -> so entao o
// desc_repetir remontava as fileiras. Dois ciclos de descoberta mais um minuto
// de espera parada.
//
// O conserto: vencido no papel + refresh na mao = nao definir o token (as
// chamadas falham de graca, local) e renovar ja no primeiro passo.
#include "traktauth.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// ---- dubles -----------------------------------------------------------------

static char arquivoTrakt[512];
static char definidoToken[300];
static int  nDefinir, nRepetir, nPostar, recusadaFake, ativoFake;
static int  postarStatus = 200;
static char postarCorpo[400];

int trakt_definir(const char *tk, const char *cli) {
  (void)cli;
  snprintf(definidoToken, sizeof definidoToken, "%s", tk ? tk : "");
  nDefinir++;
  ativoFake = 1;
  return 1;
}
int trakt_ativo(void)    { return ativoFake; }
int trakt_recusada(void) { return recusadaFake; }

const char *nuvem_trakt_cliente(void) { return "cliente-teste"; }
const char *nuvem_trakt_segredo(void) { return "segredo-teste"; }

char *dados_ler(const char *nome) {
  if (!strcmp(nome, "trakt.txt") && arquivoTrakt[0]) return strdup(arquivoTrakt);
  return NULL;
}
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_apagar(const char *n)                { (void)n; return 1; }

char *rede_postar_st(const char *url, int segundos, const char *const *cab,
                     const char *corpo, int *status) {
  (void)segundos; (void)cab;
  nPostar++;
  if (strstr(url, "/oauth/token") && corpo && strstr(corpo, "refresh_token")) {
    char *r = malloc(512);
    snprintf(r, 512, postarCorpo, (long)time(NULL));
    if (status) *status = postarStatus;
    return r;
  }
  if (status) *status = 404;
  return strdup("{}");
}

int  sessao_logada(void)        { return 0; }
int  sync_estado(void)          { return 2; }   // SYNC_PRONTO
int  sync_empurrar_credencial(const char *p, const char *j) {
  (void)p; (void)j; return 0;
}
void desc_repetir(void)         { nRepetir++; }
const char *i18n(const char *s) { return s; }

// ---- cenario ----------------------------------------------------------------

static void zera(void) {
  arquivoTrakt[0] = 0;
  definidoToken[0] = 0;
  nDefinir = nRepetir = nPostar = 0;
  recusadaFake = ativoFake = 0;
  postarStatus = 200;
  snprintf(postarCorpo, sizeof postarCorpo,
           "{\"access_token\":\"NOVO\",\"refresh_token\":\"R2\","
           "\"created_at\":%%ld,\"expires_in\":7776000}");
  traktauth_esquecer();
}

static void rodaAte(int (*cond)(void), int ms) {
  int t;
  for (t = 0; t < ms; t += 10) {
    traktauth_passo(1000 + (unsigned)t);
    if (cond()) return;
    usleep(10000);
  }
}

static int definiu(void)  { return nDefinir > 0; }
static int renovou(void)  { return nPostar > 0; }

int main(void) {
  long agora = (long)time(NULL);

  // 1. TOKEN VENCIDO + REFRESH: nao define o morto, renova no primeiro passo
  //    (agoraMs=1000 — bem antes dos 60 s que a guarda antiga exigia).
  zera();
  snprintf(arquivoTrakt, sizeof arquivoTrakt,
           "MORTO\tcliente-teste\tR1\t%ld\t60\t0", agora - 3600);
  assert(traktauth_carregar() == 1);
  assert(nDefinir == 0);                       // token morto nao vai para a rede
  rodaAte(renovou, 3000);
  assert(nPostar == 1);                        // refresh saiu sem esperar o 401
  rodaAte(definiu, 3000);
  assert(nDefinir == 1 && !strcmp(definidoToken, "NOVO"));
  assert(nRepetir == 1);                       // home remontada com Trakt vivo
  assert(traktauth_estado() == TRA_LIGADO);
  printf("ok  token vencido: renova no primeiro passo, sem definir o morto\n");

  // 2. TOKEN AINDA VALIDO: caminho de sempre, sem pedido de renovacao.
  zera();
  snprintf(arquivoTrakt, sizeof arquivoTrakt,
           "VIVO\tcliente-teste\tR1\t%ld\t7776000\t0", agora);
  assert(traktauth_carregar() == 1);
  assert(nDefinir == 1 && !strcmp(definidoToken, "VIVO"));
  rodaAte(renovou, 300);
  assert(nPostar == 0);                        // nenhum posto saiu
  printf("ok  token valido: define na hora, sem renovar\n");

  // 3. VENCIDO MAS SEM REFRESH: define mesmo assim — o 401 do primeiro ciclo
  //    e quem derruba para INVALIDO, como antes.
  zera();
  snprintf(arquivoTrakt, sizeof arquivoTrakt,
           "MORTO\tcliente-teste\t\t%ld\t60\t0", agora - 3600);
  assert(traktauth_carregar() == 1);
  assert(nDefinir == 1);                       // sem refresh, nao ha o que antecipar
  printf("ok  vencido sem refresh: define e deixa o 401 derrubar\n");

  // 4. ARQUIVO ANTIGO (2 colunas): sem datas, sem refresh — comportamento velho.
  zera();
  snprintf(arquivoTrakt, sizeof arquivoTrakt, "VELHO\tcliente-teste");
  assert(traktauth_carregar() == 1);
  assert(nDefinir == 1 && !strcmp(definidoToken, "VELHO"));
  printf("ok  arquivo de 2 colunas: define, nada de renovacao proativa\n");

  printf("traktauth_renov: tudo ok\n");
  return 0;
}
