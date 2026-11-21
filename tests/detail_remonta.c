// A pagina de titulo nao diz "catalogo remontou" quando nada remontou.
//
// No D1 (ids 15821..15897, Samsung .tpk; 15747, 1.6.5): "[detail] catalogo
// remontou: tt20285780 saiu de 0 para 0" ~3400 vezes a cada 5 min. O card de
// serie guarda o episodio no id ("tt..:1:32") e a copia na mesma posicao nao
// (ou o contrario): o strcmp de revalidarIdx falhava todo quadro, a busca por
// titulo devolvia o proprio indice e a linha saia de novo. O registro de 200 KB
// ficava so com ela.
#include "catalogo.h"
#include "detail.h"
#include "home.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static CatItem item(const char *imdb, const char *titulo, const char *tipo) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  snprintf(c.titulo, sizeof c.titulo, "%s", titulo);
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  return c;
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) detail_atualizar(0.016f, SDL_GetTicks());
}

int main(void) {
  CatItem v[3];
  HomeItem hi;
  v[0] = item("tt0000001", "Outro", "movie");
  v[1] = item("tt0052520:1:32", "Twilight Zone", "series");
  v[2] = item("tt0000002", "Silo", "series");
  cat_definir_tudo(v, 3, NULL, 0);

  memset(&hi, 0, sizeof hi);
  hi.indice = 1;
  detail_abrir(&hi);
  quadros(5);
  assert(detail_indice() == 1);

  // 1) O MESMO titulo na mesma posicao, com o id sem o episodio.
  v[1] = item("tt0052520", "Twilight Zone", "series");
  cat_definir_tudo(v, 3, NULL, 0);
  printf("--- mesmo titulo, id sem episodio\n"); fflush(stdout);
  quadros(60);
  assert(detail_indice() == 1);

  // 2) Remontagem de verdade: o titulo desce uma posicao. Segue o titulo e
  //    diz uma vez.
  { CatItem w[4];
    w[0] = item("tt0000009", "Novo", "movie");
    w[1] = v[0]; w[2] = v[1]; w[3] = v[2];
    cat_definir_tudo(w, 4, NULL, 0); }
  printf("--- remontou de verdade\n"); fflush(stdout);
  // The router (app.c) reads detail_indice() for the source search and Play
  // BEFORE detail_atualizar runs in the same frame. Read right after the
  // rebuild, it must already follow the title, not whoever took its old slot:
  // otherwise that frame asks the addons for another title's sources and Play
  // can start the wrong one (a deep-linked title, appended at the end, then
  // shifted by the start-up "Continue watching" rebuild).
  assert(detail_indice() == 2);
  assert(!strcmp(cat_item(detail_indice())->imdb, "tt0052520"));
  quadros(60);
  assert(detail_indice() == 2);
  assert(!strcmp(cat_item(detail_indice())->titulo, "Twilight Zone"));

  // 3) Id maior que 23 caracteres (canal de TV ao vivo "pp-live:..."). No D1
  //    da 1.7.0 (tizen e tizen-tpk): "pp-live:binged~ev.p-pl- saiu de 740 para
  //    741", "741 para 742"... uma linha e um cat_acrescentar POR QUADRO. O
  //    idxImdb[24] cortava o id, a busca nao achava o titulo e a copia da
  //    abertura era reinserida de novo a cada quadro.
  { CatItem w[2]; int n0;
    w[0] = item("tt0000001", "Outro", "movie");
    w[1] = item("pp-live:binged~ev.p-pl-canal-longo-123", "Canal", "tv");
    cat_definir_tudo(w, 2, NULL, 0);
    memset(&hi, 0, sizeof hi);
    hi.indice = 1;
    detail_abrir(&hi);
    n0 = cat_n();
    printf("--- id longo\n"); fflush(stdout);
    quadros(60);
    assert(cat_n() == n0);
    assert(detail_indice() == 1); }

  // 4) FICHA FECHADA: a guarda da revalidacao na leitura e `aberto`. Depois
  //    do fechamento o catalogo pode remontar sob o indice guardado sem custo
  //    nem efeito — nenhum quadro pede fonte ou Play por uma pagina fechada —
  //    e a copia da abertura NAO pode ser reinserida por uma leitura. O canal
  //    sai de 1 para 2 e o getter continua devolvendo o indice cru.
  detail_fechar();
  quadros(60);
  assert(!detail_aberto());
  { CatItem w[3]; int n0 = cat_n();
    w[0] = item("tt0000001", "Outro", "movie");
    w[1] = item("tt0000007", "Intruso", "movie");
    w[2] = item("pp-live:binged~ev.p-pl-canal-longo-123", "Canal", "tv");
    cat_definir_tudo(w, 3, NULL, 0);
    assert(cat_n() == n0 + 1); }
  assert(detail_indice() == 1);
  quadros(60);
  assert(detail_indice() == 1);
  printf("FIM\n");
  return 0;
}
