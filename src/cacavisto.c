// Adaptador do "visto" da Caca. Ver cacavisto.h: e o UNICO arquivo que muda
// quando o Diario (feat/sp-diario) entrar.
#include "cacavisto.h"
#include "catalogo.h"
#include "progresso.h"
#include <stdio.h>
#include <string.h>

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  ProgRegistro r;
  int hist;
  (void)tmdb;
  if (quando) *quando = 0;
  if (!imdb || strncmp(imdb, "tt", 2)) return 0;
  // O progresso primeiro: e o unico que sabe a data. >= 90% e o mesmo corte
  // de cat_visto para filme.
  if (prog_por_chave(imdb, &r) && r.durSeg > 60.0 && r.posSeg >= r.durSeg * 0.9) {
    if (quando && r.lastWatchedMs > 0) *quando = r.lastWatchedMs / 1000;
    return 1;
  }
  hist = cat_historico_estado_id(imdb, "movie");
  return hist == 1;
}

unsigned long long cacavisto_geracao(void) {
  return (cat_historico_geracao() << 32) ^ (unsigned long long)cat_revisao_itens();
}
