// Calls the real Next handler and player reopen with synthetic catalog data.
// It does not dispatch the UI button, select a source, or start video playback.
// The initial player has no source. Video/network boundaries use ep_close_proof.
#define main ep_close_unused_main
#include "ep_close_proof.c"
#undef main
void issue697_next(int cenario);

int main(int argc, char **argv) {
  int first = argc > 1 ? atoi(argv[1]) : 1;
  int last = argc > 1 ? first : 2;
  CHECK(first >= 1 && last <= 2);
  CHECK(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1); ling_local_legenda("none");
  for (int scenario = first; scenario <= last; scenario++) {
    CatItem v[2] = {0}; CatFileira f = {0};
    snprintf(v[0].imdb, sizeof v[0].imdb, "tt0000001");
    snprintf(v[0].titulo, sizeof v[0].titulo, "Fairy Tail");
    snprintf(v[0].tipo, sizeof v[0].tipo, "series");
    v[0].temporada = 5; v[0].episodio = 41;
    snprintf(v[1].imdb, sizeof v[1].imdb, "tt0000002");
    snprintf(v[1].titulo, sizeof v[1].titulo, "Unabomber");
    snprintf(v[1].tipo, sizeof v[1].tipo, "series");
    snprintf(f.chave, sizeof f.chave, "continue_watching"); f.ini = 0; f.n = 2;
    cat_definir_tudo(v, 2, &f, 1);
    player_abrir(0, NULL); player_definir_episodio(5, 41);
    issue697_next(scenario);
    CHECK(player_aberto());
    const CatItem *actual = cat_item(player_indice());
    if (!actual || strcmp(actual->imdb, "tt0000001"))
      fprintf(stderr, "wrong title: %s (%s)\n", actual ? actual->titulo : "<null>", actual ? actual->imdb : "");
    CHECK(actual && !strcmp(actual->imdb, "tt0000001"));
    int t, e; player_episodio_atual(&t, &e);
    CHECK(t == 5 && e == 42);
    printf("PASS scenario %d: reopened Fairy Tail with S5E42 selected (source/playback untested)\n", scenario);
    player_encerrar();
  }
  SDL_Quit(); return 0;
}
