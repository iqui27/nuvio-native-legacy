// Call the Next handler directly; inject a catalog publication after real close.
#define player_encerrar issue697_encerrar
#ifndef ISSUE697_APP
#define ISSUE697_APP "../src/app.c"
#endif
#include ISSUE697_APP
#undef player_encerrar

static int issue697_cenario;
void player_encerrar(void);
void issue697_encerrar(void) {
  player_encerrar();
  CatItem cw[2] = {0};
  snprintf(cw[0].imdb, sizeof cw[0].imdb, "tt0000002");
  snprintf(cw[0].titulo, sizeof cw[0].titulo, "Unabomber");
  snprintf(cw[0].tipo, sizeof cw[0].tipo, "series");
  if (issue697_cenario == 1) {
    snprintf(cw[1].imdb, sizeof cw[1].imdb, "tt0000001");
    snprintf(cw[1].titulo, sizeof cw[1].titulo, "Fairy Tail");
    snprintf(cw[1].tipo, sizeof cw[1].tipo, "series");
  }
  cat_trocar_continuar(cw, issue697_cenario == 1 ? 2 : 1);
}
void issue697_next(int cenario) {
  issue697_cenario = cenario;
  abrirProximoEpisodio(5, 42, 1);
}
