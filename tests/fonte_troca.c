// #570: execute the app's source-switch block with real player/catalog/progress.
#include "descoberta.h"
#include "intro.h"
#include "video.h"
static void rebuild_cw(void);
static int ready(void) { return 1; }
#define desc_refazer_continuar rebuild_cw
#define desc_episodios_garantir(i) ((void)(i))
#define intro_pedir_ids(id, tmdb, t, e, da, dp) ((void)0)
#define intro_pedir_ids_mapeados(id, tmdb, t, e, it, ie, da, dp) ((void)0)
#define video_pronto ready
#define video_url_atual() "https://example.invalid/episode.mp4"
#include "../src/player.c"
#include "dados.h"
#include <assert.h>

#define SERIES "tt0903747"
#define MOVIE "tt0111161"
static int rebuilt;
static void rebuild_cw(void) {
  CatItem cw[2] = {0};
  strcpy(cw[0].imdb, MOVIE); strcpy(cw[0].tipo, "movie");
  strcpy(cw[1].imdb, SERIES ":2:4"); strcpy(cw[1].tipo, "series");
  cw[1].temporada = 2; cw[1].episodio = 4;
  cat_trocar_continuar(cw, 2);
  rebuilt++;
}
static void consumirFolhaTocar(int *t, int *e) { (void)t; (void)e; }
static void diagnostico_cancelar_vazao(void) {}
static void switch_source(void) {
#include "fonte_troca_app.inc"
}
static void fixture(void) {
  CatItem v[3] = {0}; CatFileira rows[2] = {0};
  strcpy(v[0].imdb, SERIES ":2:4"); strcpy(v[0].tipo, "series");
  v[0].temporada = 2; v[0].episodio = 4;
  strcpy(v[1].imdb, SERIES); strcpy(v[1].tipo, "series");
  strcpy(v[2].imdb, MOVIE); strcpy(v[2].tipo, "movie");
  strcpy(rows[0].chave, "continue_watching"); rows[0].n = 1;
  strcpy(rows[1].chave, "series_top"); rows[1].ini = 1; rows[1].n = 2;
  cat_definir_tudo(v, 3, rows, 2);
  CatEp ep = {.temporada = 2, .episodio = 4};
  cat_definir_episodios(1, &ep, 1);
  player_abrir(1, NULL); player_definir_episodio(2, 4);
  comVideo = duracaoReal = 1; esperandoFonte = erroFonte = 0;
  zerarAssistido();
  posSeg = 1620; duracaoSeg = 1800; assistidoSeg = 10;
  basePos = baseDur = basePct = 0; baseTocou = 0;
}
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(""); perfis_definir_ativo(1);
  fixture();
  switch_source();
  assert(rebuilt == 1);
  assert(idbase_len(item()->imdb) == strlen(SERIES));
  assert(!strncmp(item()->imdb, SERIES, strlen(SERIES)));
  int t, e; player_episodio_atual(&t, &e);
  assert(t == 2 && e == 4);
  ProgRegistro r;
  assert(prog_por_chave(SERIES "_s2e4", &r));
  assert(r.posSeg == 1620 && r.durSeg == 1800);
  assert(vistoep_estado(SERIES, 2, 4) != 1);
  assert(!prog_por_chave(MOVIE "_s2e4", &r));
  assert(cat_item(cat_indice_por_imdb(MOVIE))->progresso == 0);
  puts("PASS: CW rebuilt during source switch; same IMDb:S:E, real resume, no watched writes");
  player_encerrar();
  // Ordinary exit still completes at 90%.
  fixture(); player_encerrar();
  assert(vistoep_estado(SERIES, 2, 4) == 1);
  puts("PASS: ordinary exit at 90% still marks watched");
  SDL_Quit();
}
