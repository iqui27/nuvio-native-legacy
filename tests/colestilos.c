// #205: "as fileiras de coleção também têm que ter todos os styles".
// A fileira de colecao oferece o MESMO conjunto de estilos da de catalogo
// (fil_estilos), cada escolha vale (fil_definir_tipo) e a Home desenha o card
// DO ESTILO — a mesma medida do catalogo, com a pasta no lugar do cartaz.
// Base: tests/colfileiras_home.c (mesma montagem da Home sem renderizador).
#define main cwordem_home_fixture_main
#include "cwordem_home.c"
#undef main
#include "../src/colfileiras.h"

static Uint32 ticks = 1000;
Uint32 SDL_GetTicks(void) { return ticks; }

int addons_n(void) { return 0; }
int addons_base_desligada(const char *b) { (void)b; return 0; }   // 203-desligados
const char *addons_base(int i) { (void)i; return ""; }
const char *addons_id_manifesto(int i) { (void)i; return ""; }
const char *sessao_usuario(void) { return "account-colestilos"; }
// Stub do mesmo porteiro de tests/home_layout.c: a regra mora em descoberta.c,
// que nao entra nesta montagem sem rede.
void desc_ocultar_vistos_atualizar(void) {}

#define GRUPO(id,pasta) "{\"id\":\"" id "\",\"title\":\"Same title\",\"folders\":[{\"id\":\"" pasta "\",\"title\":\"Folder\",\"sources\":[{\"addonBaseUrl\":\"https://collection.invalid\",\"type\":\"movie\",\"catalogId\":\"" pasta "\"}]}]}"
static const char *duas = "{\"collections\":[" GRUPO("c1","f1") "," GRUPO("c2","f2") "]}";

// A MESMA LISTA DE ESTILOS que o catalogo: mesmos tipos, na mesma ordem.
static void assertMesmosEstilos(const char *chaveCol, const char *chaveCat) {
  int tCol[FIL_TIPO_N], tCat[FIL_TIPO_N], k;
  const char *rCol[FIL_TIPO_N], *rCat[FIL_TIPO_N];
  int nCol = fil_estilos(chaveCol, tCol, rCol, FIL_TIPO_N);
  int nCat = fil_estilos(chaveCat, tCat, rCat, FIL_TIPO_N);
  assert(nCol == nCat && nCol > 4);
  for (k = 0; k < nCol; k++) {
    assert(tCol[k] == tCat[k] && !strcmp(rCol[k], rCat[k]));
    // Cada estilo da lista e SELECIONAVEL para a colecao e fica gravado nela.
    assert(fil_definir_tipo(chaveCol, tCol[k]));
    assert(fil_tipo(chaveCol) == tCol[k]);
  }
}

int main(void) {
  static CatItem items[4];
  static CatFileira rows[2];
  int k;

  fil_definir_limite(10);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true,\"homeLayout\":\"classic\"}");
  for (k = 0; k < 4; k++) {
    snprintf(items[k].imdb, sizeof items[k].imdb, "tt%d", k);
    snprintf(items[k].tipo, sizeof items[k].tipo, "movie");
    snprintf(items[k].titulo, sizeof items[k].titulo, "Title %d", k);
  }
  snprintf(rows[0].chave, sizeof rows[0].chave, "continue_watching");
  snprintf(rows[0].titulo, sizeof rows[0].titulo, "Continue");
  rows[0].ini = 0; rows[0].n = 2;
  snprintf(rows[1].chave, sizeof rows[1].chave, "addon_movie_a");
  snprintf(rows[1].titulo, sizeof rows[1].titulo, "Addon A");
  snprintf(rows[1].base, sizeof rows[1].base, "https://addon.invalid");
  snprintf(rows[1].catId, sizeof rows[1].catId, "a");
  snprintf(rows[1].tipo, sizeof rows[1].tipo, "movie");
  rows[1].ini = 2; rows[1].n = 2;
  cat_definir_tudo(items, 4, rows, 2);
  assert(colfileiras_receber(duas) == 2);
  colfileiras_sincronizar();
  sincronizarFileiras();
  assert(naHome("collection_c1") && naHome("collection_c2") && naHome("addon_movie_a"));

  // 1. A colecao oferece TODOS os estilos do catalogo, e cada um fica gravado.
  assertMesmosEstilos("collection_c1", "addon_movie_a");

  // 2. Cada estilo RENDERIZA com a medida dele na Home: o card da colecao e o
  //    do estilo (larguraDe/alturaDe de tipoDaEscolha), identico ao da fileira
  //    de catalogo vestida do mesmo estilo. O tipo continua FILEIRA_CATALOGOS
  //    — e ele que abre as pastas.
  {
    int tCol[FIL_TIPO_N];
    const char *r[FIL_TIPO_N];
    int nCol = fil_estilos("collection_c1", tCol, r, FIL_TIPO_N);
    for (k = 0; k < nCol; k++) {
      int t = tCol[k], rCol, rCat;
      const Fileira *col, *cat;
      assert(fil_definir_tipo("collection_c1", t));
      assert(fil_definir_tipo("addon_movie_a", t));
      sincronizarFileiras();
      col = naHome("collection_c1"); cat = naHome("addon_movie_a");
      assert(col && cat);
      rCol = (int)(col - fileiras); rCat = (int)(cat - fileiras);
      assert(col->tipo == FILEIRA_CATALOGOS);
      assert(col->estiloCol == (t == FIL_TIPO_AUTO ? FILEIRA_CATALOGOS : tipoDaEscolha(t)));
      if (t != FIL_TIPO_AUTO) assert(cat->tipo == tipoDaEscolha(t));
      if (t == FIL_TIPO_AUTO) {
        // Em Automatico nada muda: a forma e a da conta, a medida a dela.
        float w, h;
        assert(col->forma == col->formaAuto);
        medidaColecao(col->forma, &w, &h);
        assert(larguraFil(rCol) == w * escalaFil(rCol) && alturaFil(rCol) == h * escalaFil(rCol));
        continue;
      }
      // A pilha do Top 10 e um enfeite do catalogo (ranking de titulos): a
      // pasta renderiza no card do estilo, sem pilha.
      if (t == FIL_TIPO_TOP10) {
        assert(larguraFil(rCol) == larguraDe(FILEIRA_TOP10) * escalaFil(rCol));
        assert(alturaFil(rCol) == alturaDe(FILEIRA_TOP10) * escalaFil(rCol));
        continue;
      }
      assert(larguraFil(rCol) == larguraFil(rCat));
      assert(alturaFil(rCol) == alturaFil(rCat));
      assert(alturaFil(rCol) == alturaDe(tipoDaEscolha(t)) * escalaFil(rCol));
      assert(larguraFil(rCol) == larguraDe(tipoDaEscolha(t)) * escalaFil(rCol));
    }
  }

  // 2b. O VAO E O NUMERAL DO RANKING: a faixa dele (xOffTipo, os 138 px) segue
  //     no vao da fileira de TITULOS como sempre — sem a guarda, ela sumia de
  //     TODO mundo. A pasta nao desenha numeral: o vao da colecao e o do
  //     estilo SEM a faixa (a previa do modal segue o mesmo vao).
  {
    const Fileira *col, *cat;
    int rCol, rCat;
    assert(fil_definir_tipo("collection_c1", FIL_TIPO_RANKING));
    assert(fil_definir_tipo("addon_movie_a", FIL_TIPO_RANKING));
    sincronizarFileiras();
    col = naHome("collection_c1"); cat = naHome("addon_movie_a");
    assert(col && cat);
    rCol = (int)(col - fileiras); rCat = (int)(cat - fileiras);
    assert(cat->tipo == FILEIRA_TOP10_NUM);
    assert(col->estiloCol == FILEIRA_TOP10_NUM);
    // Titulo: o vao traz a faixa do numeral (gapDe ja a inclui).
    assert(passoFil(rCat) == larguraFil(rCat) + gapDe(FILEIRA_TOP10_NUM));
    // Pasta: o mesmo vao, menos a faixa que ela nao desenha.
    assert(passoFil(rCol) ==
           larguraFil(rCol) + gapDe(FILEIRA_TOP10_NUM) - xOffTipo(FILEIRA_TOP10_NUM));
    // Sem numeral no meio, o vao e inteiro para os dois (Cartaz).
    assert(fil_definir_tipo("collection_c1", FIL_TIPO_CARTAZ));
    assert(fil_definir_tipo("addon_movie_a", FIL_TIPO_CARTAZ));
    sincronizarFileiras();
    col = naHome("collection_c1"); cat = naHome("addon_movie_a");
    assert(col && cat);
    rCol = (int)(col - fileiras); rCat = (int)(cat - fileiras);
    assert(passoFil(rCat) == larguraFil(rCat) + gapDe(cat->tipo));
    assert(passoFil(rCol) == larguraFil(rCol) + gapDe(col->estiloCol));
  }

  // 3. A escolha PERSISTE por fileirasui.txt como a do catalogo: mesmo arquivo,
  //    mesmo campo, e a remontagem da Home devolve o estilo escolhido.
  {
    int tCol[FIL_TIPO_N];
    const char *r[FIL_TIPO_N];
    int nCol = fil_estilos("collection_c1", tCol, r, FIL_TIPO_N);
    for (k = 0; k < nCol; k++) {
      assert(fil_definir_tipo("collection_c1", tCol[k]));
      sincronizarFileiras();
      assert(naHome("collection_c1")->estiloCol ==
             (tCol[k] == FIL_TIPO_AUTO ? FILEIRA_CATALOGOS : tipoDaEscolha(tCol[k])));
    }
  }

  // 4. A conta continua mandando na forma em Automatico: sem escolha, o card e
  //    o da pasta (aqui sem tileShape, que o web le como quadrado).
  assert(fil_definir_tipo("collection_c1", FIL_TIPO_AUTO));
  sincronizarFileiras();
  assert(naHome("collection_c1")->forma == COL_FORMA_QUADRADO);

  printf("colestilos: ok\n");
  return 0;
}
