// Real grid, catalogue and detail selection; no renderer or server required.
#define desc_vertudo_n fixture_n
#define desc_vertudo_item fixture_item
#define desc_vertudo_fim fixture_fim
#define desc_vertudo_erro fixture_erro
#define desc_vertudo_mais fixture_mais
#include "../src/vertudo.c"
#include "detail.h"
#include <assert.h>

static CatItem selected;
int fixture_n(void) { return 1; }
int fixture_item(int i, CatItem *out) { if (i) return 0; *out = selected; return 1; }
int fixture_fim(void) { return 1; }
int fixture_erro(void) { return 0; }
void fixture_mais(void) {}

static void select_episode(int season, int episode) {
  selected.temporada = season; selected.episodio = episode;
  snprintf(selected.nomeEpisodio, sizeof selected.nomeEpisodio, "Episode %d", episode);
  servidorAtivo = aberta = 1; collection = NULL; tabFocus = foco = 0;
  servidorFiltro = servidorOrdem = 0; servidorLidos = -1;
  servidorOrdenar();
  SDL_Event event = {0}; event.type = SDL_KEYDOWN; event.key.keysym.sym = SDLK_RETURN;
  vertudo_evento(&event);
  int index = vertudo_pediu_abrir();
  assert(index >= 0);
  const CatItem *stored = cat_item(index);
  assert(stored->temporada == season && stored->episodio == episode);
  assert(!strcmp(stored->nomeEpisodio, selected.nomeEpisodio));
  assert(stored->progresso == 0);
  HomeItem home = {0}; home.indice = index;
  detail_abrir(&home);
  int t = 0, e = 0;
  assert(detail_ep_foco(&t, &e) && t == season && e == episode);
  detail_pedir_reproduzir();
  assert(detail_pediu_reproduzir());
  assert(detail_ep_foco(&t, &e) && t == season && e == episode);
}

int main(void) {
  const char *ids[] = {"jf:01234567.abcd", "em:01234567.abcd", "px:01234567.42"};
  for (int i = 0; i < 3; i++) {
    memset(&selected, 0, sizeof selected);
    strcpy(selected.imdb, ids[i]); strcpy(selected.tipo, "series");
    strcpy(selected.titulo, "Show");
    CatItem movie = {0}; strcpy(movie.imdb, "tt0000001"); strcpy(movie.tipo, "movie");
    cat_definir_tudo(&movie, 1, NULL, 0);
    // First insertion must work even before episode metadata arrives.
    select_episode(1, 2);
    int index = detail_indice();
    CatEp episodes[3] = {{0}};
    episodes[0].temporada = 1; episodes[0].episodio = 1;
    episodes[1].temporada = 1; episodes[1].episodio = 2;
    episodes[2].temporada = 3; episodes[2].episodio = 7;
    cat_definir_episodios(index, episodes, 3);
    detail_fechar_seco();
    // Existing series must adopt the new selection, preserving its episode list.
    select_episode(3, 7);
    assert(detail_indice() == index && cat_n() == 2);
    assert(cat_n_episodios(index) == 3);
    detail_fechar_seco();
  }
  puts("servers selection: insertion, reuse, unwatched detail and playback: OK");
  return 0;
}
