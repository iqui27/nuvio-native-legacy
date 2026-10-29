// #667: resume must wait for a source pick even when a binge group is saved.
#define SDL_MAIN_HANDLED
#define sessao_logada fixture_logged_in
#define desc_sessao_aberta fixture_discovery_open
#include "../src/app.c"
int fixture_logged_in(void) { return 1; }
int fixture_discovery_open(void) { return 1; }
#include "dados.h"
#include <assert.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  assert(argc == 2);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_dir(getenv("NUVIO_DADOS"));
  assert(ajustes_fonte_manual() == (argv[1][0] == '1'));
  CatItem c = {0};
  strcpy(c.tipo, "series"); strcpy(c.titulo, "Resume fixture");
  strcpy(c.imdb, "tt0000667");
  c.temporada = 1; c.episodio = 7; c.progresso = 35;
  cat_definir(&c, 1);
  Stream s[2] = {0};
  for (int i = 0; i < 2; i++) {
    strcpy(s[i].provedor, "Fixture");
    snprintf(s[i].url, sizeof s[i].url, "https://example.invalid/%d.mp4", i);
    strcpy(s[i].rotulo, "1080p"); s[i].altura = 1080; s[i].mp4 = 1;
    snprintf(s[i].bingeGroup, sizeof s[i].bingeGroup, "group%d", i);
  }
  stream_definir_lista(s, 2);
  assert(fontepref_guardar(c.imdb, stream_item(1)));
  player_abrir(0, NULL); player_definir_episodio(1, 7);
  tela = TELA_HOME;
  perfilSessaoAplicada = perfis_sessao_geracao();
  aguardandoFonte = 1;
  app_atualizar(0, SDL_GetTicks());
  assert(stream_preferida() == 1);
  if (ajustes_fonte_manual()) {
    assert(stream_folha_aberta() && folhaParaTocar);
    assert(aguardandoFonte == 0 && fonteEscolhida == -2);
    // Opening the sheet alone must not choose. Enter picks the saved source.
    int pick = -1;
    assert(!stream_folha_escolheu(&pick));
    SDL_Event e = {0}; e.type = SDL_KEYDOWN;
    // At the first source row, Up moves into the header; Down returns to it.
    e.key.keysym.sym = SDLK_UP; stream_folha_evento(&e);
    e.key.keysym.sym = SDLK_DOWN; stream_folha_evento(&e);
    e.key.keysym.sym = SDLK_RETURN;
    stream_folha_evento(&e);
    assert(stream_folha_escolheu(&pick) && pick == 1);
    puts("PASS: manual resume opens sheet, waits, focuses saved source");
  } else {
    assert(!stream_folha_aberta());
    assert(aguardandoFonte == 2);
    puts("PASS: automatic resume retains automatic source routing");
  }
  return 0;
}
