// Exercise the existing Trails service parser without a live account.
#define NV_REC_URL "https://fixture.invalid"
#define recomenda_cabecalhos listasux_headers
#define rede_baixar_st listasux_get
#include "../src/caca.c"
#undef recomenda_cabecalhos
#undef rede_baixar_st
#include <assert.h>
int listasux_headers(const char **cab, char *a, size_t na, char *v, size_t nv, char *p, size_t np) {
  (void)a; (void)na; (void)v; (void)nv; (void)p; (void)np;
  cab[0] = NULL; return 1;
}
char *listasux_get(const char *url, int seconds, const char *const *headers, int *status) {
  (void)seconds; (void)headers; assert(strstr(url,"/v1/trilhas")); *status=200;
  return strdup("{\"trilhas\":[{\"id\":\"oscar\",\"tipo\":\"curadoria\",\"nome\":{\"en\":\"Oscar winners\"},\"foto\":\"deploy/app/art/poster/18.jpg\"},{\"id\":\"public\",\"tipo\":\"trakt\",\"lista\":\"8842211\",\"nome\":{\"en\":\"A24 essentials\"}}]}");
}
void listasux_trails_fixture(void) {
  fazerTrilhas();
  memcpy(trilhas,trilhasNovas,sizeof trilhas); nTrilhas=nTrilhasNovas;
  nTrilhasNovas=-1; trilhasPedidas=1;
  assert(nTrilhas>3 && trilhas[1].tipo==CACA_TRILHA && !strcmp(trilhas[1].lista,"oscar"));
  assert(trilhas[2].tipo==CACA_LISTA_TRAKT && !strcmp(trilhas[2].lista,"8842211"));
}
