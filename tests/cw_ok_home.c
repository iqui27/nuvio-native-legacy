// Short OK must preserve Continue Watching's direct playback/details route.
#include "../src/home.c"
#include <assert.h>

static void short_ok(void) {
  SDL_Event event = {0};
  event.type = SDL_KEYDOWN;
  event.key.keysym.sym = SDLK_RETURN;
  home_evento(&event);
  event.type = SDL_KEYUP;
  home_evento(&event);
}

int main(void) {
  // Factory AJ_CW_OK=0 means resume playback.
  assert(ajustes_cw_ok_toca());
  CatItem episode = {0};
  strcpy(episode.imdb, "tt123:1:2");
  strcpy(episode.tipo, "series");
  strcpy(episode.titulo, "Finished episode");
  episode.temporada = 1;
  episode.episodio = 2;
  episode.progresso = 100;
  AddonRemoto addon = {.nome = "Fixture", .url = "https://fixture.invalid/manifest.json", .ativo = 1};
  assert(addons_definir_lista(&addon, 1));
  cat_definir(&episode, 1);
  assert(!homevazio_ativo(1));
  nFileiras = 1;
  focoHero = 0;
  foco.fileira = foco.coluna = 0;
  fileiras[0].n = 1;

  const int types[] = {FILEIRA_CONTINUE, FILEIRA_RETORNO, FILEIRA_NORMAL};
  for (unsigned i = 0; i < sizeof types / sizeof *types; i++) {
    fileiras[0].tipo = types[i];
    strcpy(fileiras[0].chave, i == 2 ? "continue_watching" : "resume");
    short_ok();
    assert(home_pediu_tocar() == 1);
    assert(!home_pediu_abrir());
    assert(!glance_aberto());
    assert(!home_pediu_tocar());
  }
  for (unsigned i = 0; i < sizeof types / sizeof *types; i++) {
    fileiras[0].tipo = types[i];
    strcpy(fileiras[0].chave, "upcoming_section");
    short_ok();
    assert(home_pediu_abrir() == 1);
    assert(!home_pediu_tocar());
    assert(!glance_aberto());
  }
  fileiras[0].tipo = FILEIRA_NORMAL;
  strcpy(fileiras[0].chave, "catalog");
  short_ok();
  assert(glance_aberto());
  assert(!home_pediu_tocar() && !home_pediu_abrir());
  puts("cw_ok_home: PASS");
  return 0;
}
