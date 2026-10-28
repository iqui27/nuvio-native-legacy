// Reusa o catalogo/roteiro de posplay; exercita o modulo real sem duplicar SDL.
#define main posplay_roteiro
#include "posplay.c"
#undef main

static void abrirContagem(void) {
  posplay_fechar();
  aquecer(3600, 1);
  posplay_atualizar(.016f, 1000, 3597, 3600, 1, 0, 1);
  assert(posplay_visivel());
}
static int pedir(void) {
  int t = 0, e = 0;
  int r = posplay_pediu_episodio(&t, &e);
  if (r) assert(t == 5 && e == 4);
  return r;
}
int main(void) {
  assert(posplay_roteiro() == 0);
  // The reused suite ends on a special with a post-credits scene. Restore
  // this fixture's S5E3 before testing its S5E4 countdown.
  CatItem c = {0}; CatEp eps[2] = {{0}};
  strcpy(c.imdb, "tt0944947"); strcpy(c.tipo, "series");
  c.temporada = 5; c.episodio = 3;
  eps[0].temporada = eps[1].temporada = 5;
  eps[0].episodio = 3; eps[1].episodio = 4;
  cat_definir(&c, 1); cat_definir_episodios(0, eps, 2);
  player_abrir(0, NULL); player_definir_episodio(5, 3);
  // #470: accepted neighbour-episode credits must count from card opening,
  // while 265 seconds of credits remain (Samsung reporter: 2309 / 2574).
  ajustes_shot_valor("proximoContagemLocal", 12);
  assert(posplay_contagem(1000, 0, 265, 1, 12) == 13000);
  posplay_fechar(); aquecer(2574, 1);
  posplay_atualizar(.016f, 1000, 2309, 2574, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  posplay_atualizar(.016f, 12999, 2320.999, 2574, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  posplay_atualizar(.016f, 13000, 2321, 2574, 1, 0, 1);
  assert(!posplay_visivel() && pedir() && !pedir());
  // Dismiss cancels the running countdown for the rest of this window.
  posplay_fechar(); aquecer(2574, 1);
  posplay_atualizar(.016f, 1000, 2309, 2574, 1, 0, 1);
  assert(tecla(SDLK_AC_BACK) == 1);
  posplay_atualizar(.016f, 14000, 2322, 2574, 1, 0, 1);
  assert(!posplay_visivel() && !pedir());
  // Autoplay off keeps the card manual throughout credits and EOF.
  ajustes_shot_valor("proximoAutoLocal", 1);
  posplay_fechar(); aquecer(2574, 1);
  posplay_atualizar(.016f, 1000, 2309, 2574, 1, 0, 1);
  posplay_atualizar(.016f, 14000, 2322, 2574, 1, 0, 1);
  posplay_atualizar(.016f, 300000, 2574, 2574, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  assert(tecla(SDLK_RETURN) == 1 && pedir());
  ajustes_shot_valor("proximoAutoLocal", 0);
  ajustes_shot_valor("proximoContagemLocal", 5);
  puts("posplay #470: countdown starts with card, dismiss and autoplay off preserved");
  abrirContagem();
  posplay_recuar(); posplay_aguardar_busca(1);
  posplay_atualizar(.016f, 6000, 2000, 3600, 1, 0, 0);
  posplay_atualizar(.016f, 7000, 3600, 3600, 1, 0, 1);
  posplay_aguardar_busca(0);
  posplay_atualizar(.016f, 8000, 3600, 3600, 1, 0, 1);
  assert(posplay_visivel() && !pedir()); // amostra antiga nao rearma Next
  posplay_atualizar(.016f, 9000, 2000, 3600, 1, 0, 0);
  posplay_atualizar(.016f, 10000, 3597, 3600, 1, 0, 1);
  posplay_atualizar(.016f, 13100, 3600, 3600, 1, 0, 1);
  assert(pedir()); // so fora da janela depois do seek permite nova contagem

  abrirContagem();
  posplay_atualizar(.016f, 4100, 3600, 3600, 1, 0, 1);
  assert(!posplay_visivel());
  posplay_recuar(); assert(posplay_visivel() && !pedir());
  assert(tecla(SDLK_RETURN) == 1);
  posplay_recuar(); posplay_aguardar_busca(1);
  assert(pedir() && !pedir()); // OK manual sobrevive ao cancelamento automatico

  abrirContagem();
  posplay_aguardar_busca(1);
  posplay_atualizar(.016f, 6000, 3600, 3600, 1, 0, 1);
  assert(!pedir());
  posplay_aguardar_busca(0);
  posplay_atualizar(.016f, 6100, 3600, 3600, 1, 0, 1);
  assert(pedir()); // fim real libera
  // O cancelamento tambem vale para a contagem antecipada e configuravel
  // da integracao, enquanto os creditos ainda estao rolando.
  ajustes_shot_valor("proximoContagemLocal", 12);
  posplay_fechar(); aquecer(3600, 1);
  posplay_atualizar(.016f, 1000, 3500, 3600, 1, 0, 1);
  posplay_atualizar(.016f, 5000, 3504, 3600, 1, 0, 1);
  posplay_atualizar(.016f, 12000, 3511, 3600, 1, 0, 1);
  assert(posplay_visivel() && !pedir()); // 12 s, nao os 5 s fixos do PR
  posplay_recuar();
  posplay_atualizar(.016f, 18000, 3514, 3600, 1, 0, 1);
  assert(posplay_visivel() && !pedir());
  assert(tecla(SDLK_RETURN) == 1 && pedir());
  // Pedido ja consumido nao pode ressuscitar o cartao ao cancelar uma busca.
  abrirContagem();
  posplay_atualizar(.016f, 4100, 3600, 3600, 1, 0, 1);
  assert(pedir());
  posplay_recuar(); posplay_aguardar_busca(1);
  assert(!posplay_visivel() && !pedir());
  // player_abrir fecha o pos-play e apaga o pedido do titulo anterior.
  abrirContagem();
  posplay_atualizar(.016f, 4100, 3600, 3600, 1, 0, 1);
  player_abrir(0, NULL); player_definir_episodio(5, 4);
  posplay_recuar(); posplay_aguardar_busca(1);
  assert(!posplay_visivel() && !pedir());
  puts("posplay_recuo: contagem configuravel/antecipada, pedido automatico, amostra antiga, OK manual e troca de titulo ok");
  return 0;
}
