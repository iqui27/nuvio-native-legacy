// Real shared auth builder: Nuvio refresh and JWT sizing, without network.
#define NV_REC_URL "http://fixture.invalid"
#include "../src/recomenda.c"
#include <assert.h>
static int enabled = 1, choosing, refreshed;
int ajustes_social(void) { return enabled; }
int perfis_precisa_escolher(void) { return choosing; }
const ContaPerfil *perfis_item_ativo(void) { return NULL; }
int trakt_ativo(void) { return 0; }
int trakt_cabecalhos(const char **c, char *a, size_t na, char *k, size_t nk) {
  (void)c; (void)a; (void)na; (void)k; (void)nk; return 0;
}
int sessao_token_copiar(char *t, size_t n) {
  assert(n > 900); refreshed++;
  memset(t, 'x', 900); t[900] = 0; return 1;
}
int main(void) {
  const char *cab[4]; char aut[3200], via[40], pf[40];
  assert(!recomenda_cabecalhos(cab, aut, 400, via, sizeof via, pf, sizeof pf));
  assert(recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf));
  assert(refreshed == 2 && strlen(aut) > 900 && !strcmp(via, "X-Nuvio-Auth: nuvio"));
  choosing = 1;
  assert(!recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf));
  choosing = 0; enabled = 0;
  assert(!recomenda_cabecalhos(cab, aut, sizeof aut, via, sizeof via, pf, sizeof pf));
  puts("PASS: real auth builder uses session refresh; large JWT; profile/social gates");
}
