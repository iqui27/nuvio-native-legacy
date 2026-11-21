#if defined(NV_SONDA_TEMPORADA) || defined(NV_SONDA_CARD) || \
    defined(NV_SONDA_CARD_PROG)
// Sondas da janela de recarga do catalogo (cenario da revisao Sol,
// DETAIL1431): a recarga publica n = 0 entre a primeira consulta que ainda
// ve o item e a segunda consulta de cat_item() na funcao desenhada, e conclui
// a republicacao do episodio antes do acesso ao item. Tres cenarios:
// NV_SONDA_TEMPORADA -> resumoTemporada; NV_SONDA_CARD -> nota pessoal no card
// do episodio; NV_SONDA_CARD_PROG -> barra de progresso do card no layout
// padrao com progresso vindo de extras_ep_progresso(). Duas barreiras tornam a
// janela deterministica: a instrumentacao e aplicada pelo
// detail_catalogo_ausente.sh numa copia de src/.
#include <assert.h>
#include <pthread.h>
#include <stdio.h>

#include "../src/catalogo.h"

static void esperar_catalogo_vazio(void);
static void concluir_recarga(const CatItem *consultado);
#include "../src/detail.c"

static pthread_barrier_t passos;
static pthread_t publicador;
static _Thread_local int pausar_no_zero;
static CatItem serie;
static CatEp episodio;

static void sincronizar(void) {
  int rc = pthread_barrier_wait(&passos);
  assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);
}

/* Chamada pela copia instrumentada de cat_definir_tudo logo apos n = 0. */
void sonda_temporada_n_zero(void) {
  if (!pausar_no_zero) return;
  sincronizar();
  sincronizar();
}

static void *publicar(void *unused) {
  (void)unused;
  pausar_no_zero = 1;
  sincronizar();
  cat_definir_tudo(&serie, 1, NULL, 0);
  cat_definir_episodios(0, &episodio, 1);
  return NULL;
}

static void esperar_catalogo_vazio(void) {
  assert(cat_n_episodios(idx) == 1);
  sincronizar();
  sincronizar();
  assert(cat_n() == 0);
}

static void concluir_recarga(const CatItem *consultado) {
  assert(consultado == NULL);
  sincronizar();
  assert(pthread_join(publicador, NULL) == 0);
  const CatEp *ep = cat_episodio(idx, 0);
  assert(cat_n() == 1 && ep && ep->temporada == 1 && ep->episodio == 1);
  fputs("SONDA: cat_item devolveu NULL entre a contagem e a republicação do episódio\n", stderr);
}

int main(void) {
  HomeItem home = {0};
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  snprintf(serie.imdb, sizeof serie.imdb, "tt0052520");
  snprintf(serie.titulo, sizeof serie.titulo, "Twilight Zone");
  snprintf(serie.tipo, sizeof serie.tipo, "series");
  serie.nTemporadas = 1;
  serie.temporadas[0] = 1;
  episodio.temporada = episodio.episodio = 1;
  cat_definir_tudo(&serie, 1, NULL, 0);
  cat_definir_episodios(0, &episodio, 1);
  detail_abrir(&home);
  vistoep_definir(serie.imdb, 1, 1, 0);
  assert(aberto && ehSerie() && vistoep_conhecido(serie.imdb));
  assert(secaoN(SEC_TEMPORADAS) > 0);
  assert(pthread_barrier_init(&passos, NULL, 2) == 0);
#if defined(NV_SONDA_CARD_PROG)
  // A barra so existe no layout padrao, e so passa do portao 2..97 quando o
  // progresso vem de extras_ep_progresso() (servidor pessoal), nao do item.
  extern void sonda_definir_home_layout(int v);
  extern void sonda_definir_ep_prog(int t, int e, int pct);
  sonda_definir_home_layout(HOME_LAYOUT_PADRAO);
  assert(!epModerno() && !epApple());
  sonda_definir_ep_prog(1, 1, 50);
  assert(extras_ep_progresso(1, 1) == 50);
#endif
  assert(pthread_create(&publicador, NULL, publicar, NULL) == 0);
#if defined(NV_SONDA_CARD)
  desenhaEpisodioEm((GfxRect){0, 0, 400, 300}, 0, 0, 0, 0, 0);
  assert(pthread_barrier_destroy(&passos) == 0);
  puts("PASS: card de episodio atravessou a recarga");
#elif defined(NV_SONDA_CARD_PROG)
  desenhaEpisodioEm((GfxRect){0, 0, 400, 300}, 0, 0, 0, 0, 0);
  assert(pthread_barrier_destroy(&passos) == 0);
  puts("PASS: barra de progresso do card atravessou a recarga");
#else
  resumoTemporada(0.0f, 100.0f, 0.0f);
  assert(pthread_barrier_destroy(&passos) == 0);
  puts("PASS: resumo de temporada atravessou a recarga");
#endif
  return 0;
}
#else
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
#endif
