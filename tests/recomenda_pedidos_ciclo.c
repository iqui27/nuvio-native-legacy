// OS PEDIDOS DE AMIZADE A CADA CICLO (06/10, relato do dono: "nao da pra ver
// quando alguem te adicionou, e nao mostra na ilha"). GET /v1/pedidos morava
// no bloco do relogio dos contatos (REC_CONTATOS_MS, 10 min): com a TV ligada,
// um pedido novo levava ate dez minutos para existir no app — nem a ilha nem
// a aba Amigos (recomenda_pedir_agora nao mexe naquele relogio) o viam.
//
// Roda o ciclo do fio tres vezes com o relogio dos contatos no futuro e conta
// os GET /v1/pedidos: tem de ser um por ciclo. Rede interceptada, sem servidor.
//
//   bash tests/recomenda_pedidos_ciclo.sh
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_REC_URL "http://127.0.0.1:8799"
#define rede_baixar_etag  teste_rede_etag
#define rede_postar_st    teste_rede_postar
#define rede_baixar_st    teste_rede_st
#define rede_baixar_com   teste_rede_com
#define sessao_token_copiar teste_sessao_token_copiar
#define sessao_logada teste_sessao_logada
#ifndef REC_C
#define REC_C "../src/recomenda.c"
#endif
#include REC_C

// sessao.c (#203) entrega o token por copia.
int teste_sessao_token_copiar(char *d, size_t n) { snprintf(d, n, "tok-teste"); return 1; }
int teste_sessao_logada(void) { return 1; }
static int nPedidosGet;
static char *dup(const char *s) { char *r = malloc(strlen(s) + 1); strcpy(r, s); return r; }
char *teste_rede_etag(const char *u, int seg, const char *const *cab, int *st, char *etag, unsigned te) {
  (void)u; (void)seg; (void)cab; (void)etag; (void)te; if (st) *st = 200; return dup("{}");
}
char *teste_rede_postar(const char *u, int seg, const char *const *cab, const char *c, int *st) {
  (void)u; (void)seg; (void)cab; (void)c; if (st) *st = 200; return dup("{}");
}
char *teste_rede_st(const char *u, int seg, const char *const *cab, int *st) {
  (void)seg; (void)cab;
  if (strstr(u, "/v1/pedidos") && !strstr(u, "/v1/pedidos/")) {
    nPedidosGet++;
    if (st) *st = 200;
    return dup("{\"recebidos\":[{\"pub\":\"pubnovo01\",\"apelido\":\"cine-ana\"}]}");
  }
  if (st) *st = 200;
  return dup("{}");
}
char *teste_rede_com(const char *u, int seg, const char *const *cab) { (void)u; (void)seg; (void)cab; return dup("{}"); }

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  int k;
  if (!dir || !dir[0]) { printf("pedidos_ciclo: sem NUVIO_DADOS; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("pedidos_ciclo: dados_dir errado; recusando\n"); return 2; }
  if (!mtx) mtx = SDL_CreateMutex();
  registrado = 1;                                   // ja falou com o servidor
  contatosMs = SDL_GetTicks() + REC_CONTATOS_MS;    // relogio dos contatos longe
  for (k = 0; k < 3; k++) ciclo();
  printf("GET /v1/pedidos em 3 ciclos: %d; na caixa: %d\n", nPedidosGet, recomenda_n_pedidos());
  if (nPedidosGet != 3 || recomenda_n_pedidos() != 1) { printf("pedidos_ciclo: FALHA\n"); return 1; }
  printf("pedidos_ciclo: OK\n");
  return 0;
}
