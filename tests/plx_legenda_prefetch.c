// Player real, pipeline e pre-busca falsos: sem TV, janela ou rede.
#include "mkvass.h"
#define main teste_duracao_original
#include "player_fonte_duracao.c"
#undef main

static int prePedidos, fase = 1;
int mkvass_prebuscar(const char *url, MkvassEscolher escolher, double frac) {
  assert(strstr(url, ".mkv")); assert(escolher); assert(frac == 0);
  prePedidos++; return 1;
}
int mkvass_prebusca_fase(void) { return fase; }
int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0); dados_iniciar("");
  CatItem c = {0}; strcpy(c.titulo, "Fixture"); strcpy(c.tipo, "movie");
  cat_definir(&c, 1); ling_local_legenda("pt");
  player_abrir(0, NULL);
  player_definir_fonte("https://example.invalid/film.mkv");
  printf("prefetch gate: before=4000 ms, observed=%d ms; video=%d, requests=%d\n",
         ativo ? 0 : MKVASS_PREBUSCA_MS, ativo, prePedidos);
  fflush(stdout);
  assert(prePedidos == 1); assert(ativo); assert(!player_carregando() || player_tem_video());
  quadro(); assert(ativo);
  fase = 2; quadro(); assert(ativo); assert(prePedidos == 1);
  // Falha/recusa do leitor lateral tambem nao para o pipeline.
  fase = 1; player_definir_fonte("https://example.invalid/next.mkv");
  fase = 2; quadro(); assert(ativo); assert(prePedidos == 2);
  player_voltar_a_esperar(); assert(!ativo);
  player_definir_fonte("https://example.invalid/next.mp4");
  assert(ativo); assert(prePedidos == 2);
  puts("plx subtitle prefetch: PASS");
  return 0;
}
