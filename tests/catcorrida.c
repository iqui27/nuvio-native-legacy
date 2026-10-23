// ACRESCIMO AO CATALOGO NAO PODE LER `n` FORA DA TRAVA.
//
// Queda medida no D1 (1.7.0, Samsung .tpk Tizen 9, instalacao nova, 3 quedas
// seguidas): glibc aborta com "free(): invalid pointer" logo depois de
// "[contalib] biblioteca da conta aplicada" + "[home] N fileiras na tela" +
// "[cat] continuar assistindo refeita" — tres fios trocando o bloco de itens
// ao mesmo tempo (main: contalib_reconciliar -> cat_acrescentar_lote;
// descoberta: cat_definir_tudo; Continuar: cat_trocar_continuar).
//
// cat_acrescentar_lote e cat_acrescentar calculavam o tamanho do bloco novo
// (n + qtd) ANTES de pegar pubTrava e copiavam `n` itens DEPOIS dela. Se outro
// fio publicou um catalogo maior nesse intervalo (o malloc de ~15 KB por item
// fica no meio), o memcpy escrevia alem do bloco: heap corrompido, e o abort
// aparece no proximo free de qualquer um.
//
// O gancho NV_CAT_TEST_ANTES_TRAVA roda exatamente nesse intervalo e publica
// um catalogo maior, como faria o fio da descoberta. Com SANITIZE=1 o ASan
// acusa heap-buffer-overflow no codigo antigo; sem ASan o teste confere que o
// item acrescentado esta inteiro e no indice devolvido.
//
//   SANITIZE=1 bash tests/catcorrida.sh
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
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}

#define GRANDE 40
static CatItem lote[GRANDE];
static int publicarNoGancho;

static void publicar(int qtd, int base) {
  CatFileira f;
  int i;
  memset(lote, 0, sizeof lote);
  memset(&f, 0, sizeof f);
  for (i = 0; i < qtd; i++) {
    snprintf(lote[i].imdb, sizeof lote[i].imdb, "tt%07d", base + i);
    snprintf(lote[i].titulo, sizeof lote[i].titulo, "T%d", base + i);
  }
  snprintf(f.chave, sizeof f.chave, "fileira");
  f.ini = 0; f.n = qtd;
  cat_definir_tudo(lote, qtd, &f, 1);
}

// O "outro fio": entre o calculo do tamanho e a trava, a descoberta publica
// um catalogo dez vezes maior.
void nv_cat_teste_antes_trava(void) {
  if (!publicarNoGancho) return;
  publicarNoGancho = 0;
  publicar(GRANDE, 1000);
}

static void conferirExtra(int idx, const char *imdb) {
  const CatItem *c;
  assert(idx >= 0 && idx < cat_n());
  c = cat_item(idx);
  assert(c && !strcmp(c->imdb, imdb));
  assert(cat_n() == GRANDE + 1);
}

int main(void) {
  CatItem extra;
  int idx = -1, entraram;

  // 1. cat_acrescentar_lote (contalib, busca, spotlight).
  publicar(4, 0);
  cat_quadro();
  memset(&extra, 0, sizeof extra);
  snprintf(extra.imdb, sizeof extra.imdb, "tt7777777");
  publicarNoGancho = 1;
  entraram = cat_acrescentar_lote(&extra, 1, &idx);
  assert(entraram == 1);
  conferirExtra(idx, "tt7777777");
  printf("ok  cat_acrescentar_lote com catalogo crescendo antes da trava\n");

  // 2. cat_acrescentar (detalhe, biblioteca, player, vertudo).
  publicar(4, 0);
  cat_quadro();
  snprintf(extra.imdb, sizeof extra.imdb, "tt8888888");
  publicarNoGancho = 1;
  idx = cat_acrescentar(&extra);
  conferirExtra(idx, "tt8888888");
  printf("ok  cat_acrescentar com catalogo crescendo antes da trava\n");

  // Mutacoes de episodio/progresso revalidam o indice DEPOIS da trava.
  // O bloco cresce de 4 para 40 no gancho: 39 agora e 39, nao 39 % 4.
  publicar(4, 0);
  publicarNoGancho = 1;
  cat_apontar_episodio(39, 2, 3);
  assert(!publicarNoGancho && cat_n() == GRANDE);
  assert(cat_item(39)->temporada == 2 && cat_item(39)->episodio == 3);
  assert(cat_item(3)->temporada == 0);
  printf("ok  apontar episodio revalida o indice sob a trava\n");

  publicar(4, 0);
  publicarNoGancho = 1;
  cat_aplicar_progresso(39, 600, 3000, 2, 4);
  assert(!publicarNoGancho && cat_n() == GRANDE);
  assert(cat_item(39)->progresso == 20 && cat_item(39)->episodio == 4);
  assert(cat_item(3)->progresso == 0);
  printf("ok  aplicar progresso revalida o indice sob a trava, sem trava recursiva\n");

  cat_quadro();
  cat_quadro();
  printf("catcorrida: tudo ok\n");
  return 0;
}
