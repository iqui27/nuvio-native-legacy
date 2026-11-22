// #784: manual P2P selection through the real stream resolver and app update.
#define SDL_MAIN_HANDLED
#define sessao_logada fixture_logged_in
#define desc_sessao_aberta fixture_discovery_open
#define p2p_ativo fixture_p2p_ativo
#define debrid_ativo fixture_debrid_ativo
#include "../src/app.c"
#include "dados.h"
#include <assert.h>

static int pedidos, abriu;
static char hashPedido[80], urlAberta[512];
static int arquivoPedido;
int fixture_logged_in(void) { return 1; }
int fixture_discovery_open(void) { return 1; }
int fixture_p2p_ativo(void) { return 1; }
int fixture_debrid_ativo(void) { return 0; }
int fixture_video_tocar(const char *url) {
  abriu++;
  snprintf(urlAberta, sizeof urlAberta, "%s", url);
  return 1;
}
int fixture_p2p_resolver(const char *hash, int arquivo, const char *fontes, char *url, unsigned n) {
  (void)fontes;
  pedidos++;
  snprintf(hashPedido, sizeof hashPedido, "%s", hash);
  arquivoPedido = arquivo;
  snprintf(url, n, "http://127.0.0.1/fixture.mp4");
  return P2P_OK;
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  CatItem c = {0};
  strcpy(c.tipo, "movie"); strcpy(c.titulo, "Fixture"); strcpy(c.imdb, "tt0000784");
  cat_definir(&c, 1);
  Stream s[2] = {0};
  addonmodo_carregar("fixture", 1);
  assert(addonmodo_set(784, AM_P2P));
  for (int i = 0; i < 2; i++) {
    s[i].addonIdentidade = 784;
    strcpy(s[i].provedor, "Fixture");
    snprintf(s[i].infoHash, sizeof s[i].infoHash, "%040d", i + 1);
    s[i].fileIdx = i + 4;
  }
  stream_definir_lista(s, 2);
  assert(stream_n() == 2);
  player_abrir(0, NULL);
  tela = TELA_HOME;
  perfilSessaoAplicada = perfis_sessao_geracao();
  while (!SDL_GetTicks()) SDL_Delay(1); // Zero means no manual watchdog.
  tocarFonteEscolhida(1, 1);
  assert(fioTorrentVivo);
  alarm(15);
  while (atomic_load_explicit(&torrentJob.estado, memory_order_acquire) != 2) sched_yield();
  app_atualizar(0, SDL_GetTicks());
  assert(pedidos == 1 && arquivoPedido == 5 && !strcmp(hashPedido, s[1].infoHash));
  assert(abriu == 1 && !strcmp(urlAberta, "http://127.0.0.1/fixture.mp4"));
  assert(stream_atual() == 1 && fonteManualDesde);
  player_erro_fonte();
  assert(player_fonte_falhou());
  for (int i = 0; i < 3; i++) app_atualizar(0, SDL_GetTicks());
  assert(!fonteManualDesde && stream_atual() == 1 && pedidos == 1 && abriu == 1);
  assert(fontesmortas_idade(stream_item(1)) >= 0 && fontesmortas_idade(stream_item(0)) < 0);
  // An explicit retry may use the cached external-server URL, but never picks
  // the other torrent or silently invokes the P2P resolver for it.
  tocarFonteEscolhida(1, 1);
  app_atualizar(0, SDL_GetTicks());
  assert(stream_atual() == 1 && pedidos == 1 && abriu == 2);
  assert(fontesmortas_idade(stream_item(1)) >= 0);
  assert(!strcmp(urlAberta, "http://127.0.0.1/fixture.mp4"));
  alarm(0);
  puts("PASS: manual P2P resolver, player handoff, failure without failover");
  // Os fios de infra do app (rede, addons) nao tem parada: fora do
  // sandbox-exec um deles corria com o exit() e derrubava o processo
  // DEPOIS do PASS. _exit encerra sem a demoliçao que nao esta em prova.
  fflush(stdout);
  _exit(0);
}
