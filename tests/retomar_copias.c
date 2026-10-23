// ISSUE #208: "Retomar" so na copia do filme que guardou o progresso.
//
// O mesmo filme costuma estar em duas fileiras: "Continuar assistindo" no topo
// e uma fileira de catalogo (Populares, Em alta...). O progresso vive em cada
// CatItem (itens[i].progresso), e o botao do detalhe e o player leem o da
// copia aberta. aplicarProgressoDoDisco dava cada registro de progresso.c so
// a PRIMEIRA copia que casava (a do CW, que vem antes) e cat_salvar_progresso_ep
// so a copia que tocou: aberta pelo tile da outra fileira, a pagina dizia
// "Reproduzir" e o player comecava do zero.
//
//   bash tests/retomar_copias.sh
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int         ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }

// progresso.c falso: um registro de filme (Um Sonho de Liberdade, 40%) e um
// de serie (T2E3).
static int nRegs = 2;
int prog_ler(ProgRegistro *saida, int max) {
  int k = 0;
  if (max < 2) return 0;
  memset(saida, 0, sizeof(ProgRegistro) * 2);
  if (nRegs > 0) {
    snprintf(saida[0].contentId, sizeof saida[0].contentId, "tt0111161");
    snprintf(saida[0].tipo, sizeof saida[0].tipo, "movie");
    saida[0].posSeg = 3456; saida[0].durSeg = 8640; saida[0].lastWatchedMs = 1000;
    k = 1;
  }
  if (nRegs > 1) {
    snprintf(saida[1].contentId, sizeof saida[1].contentId, "tt0903747");
    snprintf(saida[1].tipo, sizeof saida[1].tipo, "series");
    saida[1].temporada = 2; saida[1].episodio = 3;
    saida[1].posSeg = 600; saida[1].durSeg = 3000; saida[1].lastWatchedMs = 900;
    k = 2;
  }
  return k;
}
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 1;
}

static CatItem lote[6];
static CatFileira fil[2];

static void item(int i, const char *imdb, const char *tipo, const char *titulo) {
  snprintf(lote[i].imdb, sizeof lote[i].imdb, "%s", imdb);
  snprintf(lote[i].tipo, sizeof lote[i].tipo, "%s", tipo);
  snprintf(lote[i].titulo, sizeof lote[i].titulo, "%s", titulo);
}

static int cwProg; static long long cwMs;
static void publicar(void) {
  memset(lote, 0, sizeof lote);
  memset(fil, 0, sizeof fil);
  // fileira 0: Continuar assistindo
  item(0, "tt0111161", "movie", "Um Sonho de Liberdade");
  item(1, "tt0903747", "series", "Breaking Bad");
  lote[0].progresso = cwProg; lote[0].retomadoMs = cwMs;
  if (cwProg) lote[0].restanteMin = 30;
  // fileira 1: catalogo comum, com as MESMAS obras
  item(2, "tt0068646", "movie", "O Poderoso Chefao");
  item(3, "tt0111161", "movie", "Um Sonho de Liberdade");
  item(4, "tt0903747", "series", "Breaking Bad");
  item(5, "tt1375666", "movie", "A Origem");
  snprintf(fil[0].chave, sizeof fil[0].chave, "continue_watching");
  fil[0].ini = 0; fil[0].n = 2;
  snprintf(fil[1].chave, sizeof fil[1].chave, "top");
  fil[1].ini = 2; fil[1].n = 4;
  cat_definir_tudo(lote, 6, fil, 2);
}

int main(void) {
  publicar();
  printf("cw=%d tile=%d (filme)\n", cat_item(0)->progresso, cat_item(3)->progresso);
  assert(cat_item(0)->progresso == 40);
  // O tile da fileira de catalogo e o que o #208 abre.
  assert(cat_item(3)->progresso == 40);
  printf("ok  progresso do disco chega no tile do filme na fileira de catalogo\n");
  assert(cat_item(4)->temporada == 2 && cat_item(4)->episodio == 3 && cat_item(4)->progresso == 20);
  printf("ok  e no tile da serie, com o episodio\n");
  assert(cat_item(2)->progresso == 0 && cat_item(5)->progresso == 0);
  printf("ok  titulos sem registro continuam zerados\n");

  // Assistir pelo tile (indice 3) e voltar: a copia do CW tambem anda.
  cat_salvar_progresso_ep(3, 6480, 8640, 0, 0, -1);
  assert(cat_item(3)->progresso == 75);
  assert(cat_item(0)->progresso == 75);
  printf("ok  gravar numa copia atualiza as outras copias da mesma obra\n");
  assert(cat_item(2)->progresso == 0);

  // "Assistir do comeco" / tirar: zerar uma copia nao deixa a outra retomando.
  cat_zerar_progresso(0);
  assert(cat_item(0)->progresso == 0);
  assert(cat_item(3)->progresso == 0);
  printf("ok  zerar uma copia zera as outras\n");

  // Card do CW montado do Trakt/conta, SEM registro local (visto no celular):
  // o tile herda o progresso dele.
  nRegs = 0; cwProg = 55; cwMs = 5000;
  publicar();
  printf("cw=%d tile=%d (so remoto)\n", cat_item(0)->progresso, cat_item(3)->progresso);
  assert(cat_item(3)->progresso == 55 && cat_item(3)->restanteMin == 30);
  printf("ok  sem registro local o tile herda o card remoto do CW\n");

  // Card do Trakt MAIS NOVO que o registro do disco: nenhuma copia volta no
  // tempo, o tile fica com o do Trakt e nao com o 40%% velho do disco.
  nRegs = 2; cwProg = 60; cwMs = 5000;
  publicar();
  printf("cw=%d tile=%d (trakt mais novo)\n", cat_item(0)->progresso, cat_item(3)->progresso);
  assert(cat_item(0)->progresso == 60 && cat_item(3)->progresso == 60);
  printf("ok  o instante decide pela obra: o tile segue o card mais novo\n");

  printf("retomar_copias: tudo ok\n");
  return 0;
}
