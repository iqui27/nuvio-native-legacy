// Exercise the production list worker with the WASM transport contract.
#include "rede.h"
#include "trakt.h"
#include "listasescrita.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static int posts, putCount, strict;
int trakt_cabecalhos(const char **cab, char *aut, size_t na, char *key, size_t nk) {
  (void)na; (void)nk;
  strcpy(aut, "Authorization: Bearer fixture"); strcpy(key, "trakt-api-key: fixture");
  cab[0] = aut; cab[1] = "trakt-api-version: 2"; cab[2] = key; cab[3] = NULL;
  return 1;
}
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  (void)p; strict++; r->erro = REDE_INDISPONIVEL; return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
static char *reply(const char *const *cab, const char *body, int *status) {
  assert(cab[0] && strstr(cab[0], "Authorization:")); assert(body && body[0] == '{');
  *status = 204; return strdup("");
}
char *rede_postar_st(const char *u, int t, const char *const *cab, const char *body, int *st) {
  (void)u; assert(t == 20); posts++; return reply(cab, body, st);
}
char *rede_putar_st(const char *u, int t, const char *const *cab, const char *body, int *st) {
  (void)u; assert(t == 20); putCount++; return reply(cab, body, st);
}
static void confirmed(int started) {
  assert(started); lstesc_teste_esperar(); assert(lstesc_consumir() == LSTESC_OK);
}
int main(void) {
  confirmed(lstesc_criar("Fixture", "", 1));
  confirmed(lstesc_descricao(77, "description"));
  confirmed(lstesc_adicionar(77, "tt101", "movie", NULL));
  confirmed(lstesc_ordenar(77, (long[]){101,102,100}, 3));
  confirmed(lstesc_nota(77, 101, "note"));
  assert(posts == 3 && putCount == 2 && strict == 0);
}
