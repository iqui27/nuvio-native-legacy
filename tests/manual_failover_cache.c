// F67: a semeadura manual tem de usar o mesmo estado de cache que a folha
// usou para exibir a fonte. Em "Em cache", uma fonte HTTP com cache
// confirmado escolhida a dedo semeia a sessao (a folha mostra via
// stream_filtro_dados; o snapshot s->filtro do parse nao tem o estado vivo)
// e a fonte nativa confirmada depois da escolha segue elegivel ao failover.
#include "streams.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
const char *i18n(const char *s) { return s; }
void wsenvio_podar(void) {}
#include "../src/streams.c"

static int cenario(unsigned cache) {
  // Fonte HTTP no mesmo estado em que o parser a entrega: o estado de
  // cache confirmado mora nos campos vivos, nao no snapshot filtro.
  static Stream fontes[2];
  memset(fontes, 0, sizeof fontes);
  Stream *http = &fontes[0], *nativa = &fontes[1];
  snprintf(http->url, sizeof http->url, "https://fixture.invalid/cached.mp4");
  snprintf(http->addonChave, sizeof http->addonChave, "fixture");
  http->filtroLido = 1;
  http->emCache = 1;
  http->cacheNativo = 1;
  snprintf(nativa->infoHash, sizeof nativa->infoHash, "ABCDEF0123456789");
  nativa->fileIdx = 3;
  snprintf(nativa->addonChave, sizeof nativa->addonChave, "fixture");
  nativa->filtroLido = 1;
  lista = fontes;
  n = 2;

  FonteFalhaConfig config;
  fontefalha_config(&config);
  config.addons = 1;
  config.provedores = 1;
  config.filtro.cache = cache;
  assert(fontefalha_filtro_definir(&config.filtro));
  FonteFalhaSessao *sessao = fontefalha_sessao_nova(&config, 15, 0);
  assert(sessao);

  int visible = stream_filtro_permite(http);
  int seeded = stream_falha_semear_manual(sessao, 0, listaGeracao, http->url);
  int attempts = fontefalha_tentativas(sessao);

  // Fonte nativa confirmada pelo resolvedor DEPOIS da escolha manual: o
  // failover tem de oferece-la pelo mesmo estado de cache vivo.
  nativa->emCache = 1;
  nativa->cacheNativo = 1;
  char id[sizeof nativa->url + sizeof nativa->cabecalhos + 48];
  FonteFalhaCandidata c = {0};
  stream_falha_identidade(nativa, id, sizeof id);
  c.identidade = id;
  c.indice = 1;
  c.nativa = 1;
  stream_filtro_dados(nativa, &c.filtro);
  snprintf(c.addon, sizeof c.addon, "%s", nativa->addonChave);
  FonteFalhaRota rota;
  int prox = fontefalha_proxima(sessao, &c, 1, 1, &rota);
  fontefalha_sessao_liberar(sessao);
  printf("cache=%u visible=%d seeded=%d attempts=%d prox=%d indice=%d servico=%d\n",
         cache, visible, seeded, attempts, prox,
         prox == FONTEFALHA_PROXIMA ? rota.indice : -1,
         prox == FONTEFALHA_PROXIMA ? rota.servico : -1);
  return visible == 1 && seeded == 1 && attempts == 1 &&
         prox == FONTEFALHA_PROXIMA && rota.indice == 1 && rota.servico >= 0;
}

int main(int argc, char **argv) {
  if (argc != 2) return 2;
  fontefalha_dir(argv[1]);
  int okTodas = cenario(FONTEFILTRO_CACHE_TODAS);
  int okCache = cenario(FONTEFILTRO_CACHE_SIM);
  return okTodas && okCache ? 0 : 1;
}
