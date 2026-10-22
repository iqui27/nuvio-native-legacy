// A pagina de uma serie pede a lista de episodios quando a faixa dela esta
// vazia — na abertura e depois de a faixa ser apagada sem a revisao andar.
//
// TCL do dono, 2.0.3 (08/10): a pagina de Silo aberta pela ilha (o mesmo
// titulo que estava tocando) ficava sem os cartoes de episodio, e o player
// dizia "[posplay] sem lista de episodios". Dois buracos:
//   1) quem pedia a lista na abertura era so app.c, e so quando o alvo de
//      fontes MUDA — reabrir a serie que tocava tem o mesmo alvo;
//   2) a volta do vetor comum de episodios (cat_definir_episodios com
//      nEps + qtd > CAT_EP_MAX) zera as faixas de todos os titulos sem subir
//      cat_revisao, e a pagina so repedia quando a revisao andava. Desde
//      1a5d1670 a troca de catalogo nao zera mais nEps, entao a volta passou a
//      acontecer a cada 1200 episodios publicados na sessao.
// Antes do conserto as duas verificacoes abaixo falham.
#include "catalogo.h"
#include "descoberta.h"
#include "detail.h"
#include "home.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void eps(CatEp *v, int n) {
  int i;
  memset(v, 0, sizeof *v * (size_t)n);
  for (i = 0; i < n; i++) { v[i].temporada = 1 + i / 100; v[i].episodio = 1 + i % 100; }
}

int main(void) {
  CatItem v[3];
  HomeItem hi;
  CatEp *lote = malloc(sizeof(CatEp) * 1200);
  int falhas = 0;
  assert(lote);
  // ids que nenhum servico conhece: o pedido sai, demora na rede e nao
  // publica nada enquanto o teste confere.
  v[0] = item("tt0000001", "Outro", "movie");
  v[1] = item("tt9999901:3:5", "Serie Aberta", "series");
  v[2] = item("tt9999902", "Outra Serie", "series");
  cat_definir_tudo(v, 3, NULL, 0);

  // 1) ABERTURA COM A FAIXA VAZIA, sem app.c no laco (o caso "mesmo alvo").
  memset(&hi, 0, sizeof hi);
  hi.indice = 1;
  detail_abrir(&hi);
  quadros(1);
  if (!desc_episodios_carregando(1)) {
    printf("FALHA 1: pagina de serie aberta sem episodios nao pediu a lista\n");
    falhas++;
  } else printf("ok 1: a pagina pediu a lista na abertura\n");
  // Espera o pedido terminar (rede sem resposta para estes ids).
  { int k; for (k = 0; k < 400 && desc_episodios_carregando(1); k++) SDL_Delay(25); }

  // 2) A VOLTA DO VETOR APAGA A LISTA SEM SUBIR A REVISAO.
  eps(lote, 3);
  cat_definir_episodios(1, lote, 3);
  quadros(3);
  assert(cat_n_episodios(1) == 3);
  { unsigned rev = cat_revisao();
    eps(lote, 1200);
    cat_definir_episodios(2, lote, 1200);   // nEps + 1200 > CAT_EP_MAX: volta
    assert(cat_revisao() == rev);            // a revisao nao andou...
    assert(cat_n_episodios(1) == 0); }       // ...e a lista da serie aberta sumiu
  quadros(1);
  if (!desc_episodios_carregando(1)) {
    printf("FALHA 2: lista apagada pela volta do vetor e a pagina nao repediu\n");
    falhas++;
  } else printf("ok 2: a pagina repediu depois da volta do vetor\n");
  free(lote);
  printf(falhas ? "FALHOU\n" : "FIM\n");
  fflush(stdout);
  _exit(falhas ? 1 : 0);
}
