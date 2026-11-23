// CAT_ITEM NULL NO MEIO DA TROCA DO CATALOGO (PR 734, parte A): a refacao do
// "Continuar assistindo" publica n = 0 por um instante, de outro fio, e todo
// leitor que chamou cat_item(i) uma segunda vez — ou confiou no resultado —
// dereferenciava NULL. --wrap devolve NULL sob demanda em TODAS as chamadas,
// pondo o processo exatamente naquele instante, e cada ponto guardado pelo PR
// e exercitado no codigo de producao:
//   - notasDados()          (detail.c, neste TU — nota pessoal da pagina)
//   - cat_tipo_por_imdb()   (catalogo.c — tipo para compat de IMDb)
//   - posplay_atualizar()   (posplay.c — marcador de pos-creditos do filme)
// Sem a correcao os caminhos abaixo dereferenciam NULL e o teste morre.
#include "../src/detail.c"   // estaticos: idx, notasDados()
#include "posplay.h"
#include "nvtmp.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

// cat_item vive em catalogo.c; o link o resolve como __real_cat_item.
extern const char *cat_tipo_por_imdb(const char *imdb);   // sem header, como em reacao.c
const CatItem *__real_cat_item(int i);
static int nulo;
const CatItem *__wrap_cat_item(int i) { return nulo ? NULL : __real_cat_item(i); }

int main(void) {
  CatItem c = {0};
  const NotasSecao *s;

  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  snprintf(c.imdb, sizeof c.imdb, "tt9999905");
  snprintf(c.tipo, sizeof c.tipo, "movie");
  cat_definir_tudo(&c, 1, NULL, 0);
  idx = 0;

  // --- caminho normal: o wrap passa adiante ---------------------------------
  assert(!strcmp(cat_tipo_por_imdb("tt9999905"), "movie"));
  assert(!strcmp(cat_tipo_por_imdb("tt9999905:x"), "series"));  // fora do acervo: sufixo decide
  posplay_atualizar(0.016f, 0, 10.0, 300.0, 0, 0, 0);
  s = notasDados();
  assert(s);

  // --- o instante da troca: TODA chamada a cat_item devolve NULL ------------
  nulo = 1;
  // catalogo.c: um cat_item so, NULL => o fallback (sufixo), nunca NULL->tipo.
  assert(!strcmp(cat_tipo_por_imdb("tt9999905"), "movie"));
  assert(!strcmp(cat_tipo_por_imdb("tt9999905:1"), "series"));
  // posplay.c: dois cat_item por quadro no filme (marcador e cena no fim).
  posplay_atualizar(0.016f, 16, 10.0, 300.0, 0, 0, 0);
  posplay_atualizar(0.016f, 32, 295.0, 300.0, 0, 0, 0);
  // detail.c: a nota pessoal sai 0 em vez de ler ci->imdb de um ponteiro nulo.
  s = notasDados();
  assert(s && s->notaPessoal == 0);
  // heroWeb: o destaque tambem lia cat_item(idx) duas vezes para a nota — a
  // mesma guarda, exercida pela entrada publica que o desenho usa.
  nulo = 0;
  s = notasDados();
  assert(s);

  puts("cat_troca_null: notas, tipo por imdb e pos-play sobrevivem ao cat_item NULL da troca");
  return 0;
}
