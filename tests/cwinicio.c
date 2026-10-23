// #205: remote CW bar and manual-source resume must describe the same episode.
// Real catalog/progress/player; only background metadata requests are disabled.
#include "descoberta.h"
#include "intro.h"
#define desc_episodios_garantir(i) ((void)(i))
#define intro_pedir_ids(id, tmdb, t, e, da, dp) ((void)0)
#define intro_pedir_ids_mapeados(id, tmdb, t, e, it, ie, da, dp) ((void)0)
#include "../src/player.c"
#include "dados.h"
#include "perfis.h"
#include <assert.h>

#define SERIE "tt0903747"

static void publicar(int temporada, int episodio, int pct, long long ms) {
  CatItem v[3] = {0};
  CatFileira f[2] = {0};
  snprintf(v[0].imdb, sizeof v[0].imdb, SERIE ":%d:%d", temporada, episodio);
  snprintf(v[0].tipo, sizeof v[0].tipo, "series");
  v[0].temporada = temporada; v[0].episodio = episodio;
  v[0].progresso = pct; v[0].retomadoMs = ms; v[0].restanteMin = 20;
  snprintf(v[1].imdb, sizeof v[1].imdb, SERIE);
  snprintf(v[1].tipo, sizeof v[1].tipo, "series");
  snprintf(v[2].imdb, sizeof v[2].imdb, "tt0111161");
  snprintf(v[2].tipo, sizeof v[2].tipo, "movie");
  snprintf(f[0].chave, sizeof f[0].chave, "continue_watching");
  f[0].ini = 0; f[0].n = 1;
  snprintf(f[1].chave, sizeof f[1].chave, "series_top");
  f[1].ini = 1; f[1].n = 2;
  cat_definir_tudo(v, 3, f, 2);
}

static void remoto(int temporada, int episodio, int pct, long long ms) {
  CatItem cw = *cat_item(0);
  snprintf(cw.imdb, sizeof cw.imdb, SERIE ":%d:%d", temporada, episodio);
  cw.temporada = temporada; cw.episodio = episodio;
  cw.progresso = pct; cw.retomadoMs = ms; cw.restanteMin = 15;
  cat_trocar_continuar(&cw, 1);
}

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1);
  // Old local episode; newer remote playback has ONLY a percentage.
  assert(prog_gravar_local(SERIE, 2, 3, 600, 1800, -1));
  ProgRegistro r;
  assert(prog_por_chave(SERIE "_s2e3", &r));
  publicar(2, 3, 33, r.lastWatchedMs);
  assert(cat_item(1)->progresso == 33);
  remoto(2, 4, 55, r.lastWatchedMs + 1000);
  assert(cat_item(0)->progresso == 55 && cat_item(0)->episodio == 4);
  assert(!prog_por_chave(SERIE "_s2e4", &r));

  // CW OK: app.c opens the tile and explicitly keeps its S2E4 target.
  player_abrir(0, NULL); player_definir_episodio(2, 4);
  assert(retomarPct == 55);
  player_encerrar();
  // Episode metadata is on the catalog copy. player_indice() prefers it;
  // app.c's manual source selection closes/reopens on that returned index.
  CatEp ep = {0}; ep.temporada = 2; ep.episodio = 4;
  snprintf(ep.nome, sizeof ep.nome, "Episode four");
  cat_definir_episodios(1, &ep, 1);
  player_abrir(0, NULL); player_definir_episodio(2, 4);
  int titulo = player_indice(), t, e;
  assert(titulo == 1);
  player_episodio_atual(&t, &e); player_encerrar();
  player_abrir(titulo, NULL); player_definir_episodio(t, e);
  printf("CW S2E4 bar=%d%%; manual-source resume=%d%% (1800s -> %.0fs)\n",
         cat_item(0)->progresso, retomarPct, 1800.0 * retomarPct / 100.0);
  fflush(stdout);
  assert(retomarPct == 55); // Before fix: 0%, despite the CW tile's 55% bar.
  assert(cat_item(1)->temporada == 2 && cat_item(1)->episodio == 4);
  assert(cat_item(1)->retomadoMs == cat_item(0)->retomadoMs);
  player_encerrar();

  // Same episode, newer percent also replaces an already nonzero copy.
  remoto(2, 4, 70, cat_item(0)->retomadoMs + 1000);
  assert(cat_item(1)->progresso == 70);
  // A stale response must NOT replace the newer copy (or leak into movies).
  remoto(2, 4, 40, cat_item(1)->retomadoMs - 1);
  assert(cat_item(0)->progresso == 70 && cat_item(1)->progresso == 70);
  assert(cat_item(2)->progresso == 0);
  SDL_Quit();
  puts("cwinicio: PASS (remote-only episode, manual source, newest copy wins)");
}
