#include "../src/detail.c"
#include <assert.h>

int main(void) {
  CatItem serie = {0};
  CatEp ep = {0};
  HomeItem hi = {0};
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  snprintf(serie.imdb, sizeof serie.imdb, "tt0052520");
  snprintf(serie.titulo, sizeof serie.titulo, "Twilight Zone");
  snprintf(serie.tipo, sizeof serie.tipo, "series");
  serie.nota = 84;
  serie.nTemporadas = 1;
  serie.temporadas[0] = 1;
  ep.temporada = ep.episodio = 1;
  ep.nota = 81;
  cat_definir_tudo(&serie, 1, NULL, 0);
  cat_definir_episodios(0, &ep, 1);
  detail_abrir(&hi);
  reacao_nota_definir(serie.imdb, 8);
  assert(notasDados()->notaPessoal == 8);
  assert(ehSerie() && notasCatDisponiveis());

  for (int rodada = 0; rodada < 3; rodada++) {
    cat_definir_tudo(NULL, 0, NULL, 0);
    assert(cat_item(idx) == NULL);
    assert(cat_indice_por_imdb(serie.imdb) == -1);
    detail_atualizar(0.016f, SDL_GetTicks());
    assert(aberto);
    assert(notasDados()->notaPessoal == 0);
    assert(notasDados()->nTemp == 0);
    assert(notaDe(idx) == 0 && !ehSerie());
    assert(!notasCatDisponiveis());
    assert(notasCatPorNumero(1, 1) == NULL);
    assert(notasCatNEps(0) == 0 && notasCatNota(0, 0) == 0);

    cat_definir_tudo(&serie, 1, NULL, 0);
    cat_definir_episodios(0, &ep, 1);
    detail_atualizar(0.016f, SDL_GetTicks());
    assert(aberto && ehSerie());
    assert(notasDados()->notaPessoal == 8);
    assert(notasDados()->nTemp == 1);
    assert(notasDados()->tempNum(0) == 1);
    assert(notasDados()->nEps(0) == 1);
    assert(notasDados()->epNum(0, 0) == 1);
    assert(notasDados()->epNota(0, 0) == 81);
  }
  puts("PASS: detalhe sobrevive ao catalogo ausente e recupera notas e episodios");
  return 0;
}
