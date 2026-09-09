// AS FAIXAS DE EPISODIO SOBREVIVEM AO CATALOGO CRESCER.
//
// Os episodios nao vivem num vetor por titulo: ha um vetor unico e cada titulo
// guarda uma janela (epIni, epQtd) sobre ele — a mesma forma das fileiras, e a
// mesma armadilha. Quem acrescenta um titulo no FIM nao muda o indice de
// ninguem, entao nada ali pode invalidar a janela dos outros.
//
// Era o defeito: garantirFaixas() zerava o vetor inteiro e era chamada tambem
// por cat_acrescentar e pelo append em lote. Na TV isso apagava os episodios da
// serie ABERTA assim que a descoberta acrescentava qualquer titulo, e a secao
// de episodios desaparecia da pagina de detalhe — o D-pad passava de
// "Temporadas" direto para as abas, porque secao com zero colunas e
// intransponivel.
//
// O QUE ESTE TESTE PROVA:
//   1. acrescentar UM titulo nao mexe nos episodios de quem ja estava;
//   2. acrescentar um LOTE tambem nao;
//   3. o titulo NOVO nasce com zero episodios, e nao com lixo do realloc;
//   4. trocar o catalogo inteiro (cat_definir_tudo) INVALIDA tudo, porque ai os
//      indices mudaram de verdade — o contrario dos casos acima.
//
//   bash tests/cateps.sh
#include "../src/catalogo.h"
#include "../src/progresso.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// --- DUBLES: nenhum participa da regra, so fazem catalogo.c linkar ----------
int         ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s)         { return s; }
const char *dados_dir(void)             { return ""; }
const char *sessao_usuario(void)        { return ""; }
int         perfis_ativo(void)          { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

static CatItem itens[3];

static void montar(void) {
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < 3; i++) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "T%d", i);
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "series");
  }
  cat_definir_tudo(itens, 3, NULL, 0);
}

// Tres episodios com nome reconhecivel, para a assercao ser sobre CONTEUDO e
// nao so sobre a contagem: um vetor com o tamanho certo e os dados de outra
// serie passaria por um teste que so contasse.
static void publicar(int alvo, const char *prefixo, int quantos) {
  CatEp eps[4];
  int i;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < quantos; i++) {
    eps[i].temporada = 1;
    eps[i].episodio = i + 1;
    snprintf(eps[i].nome, sizeof eps[i].nome, "%s-E%d", prefixo, i + 1);
  }
  cat_definir_episodios(alvo, eps, quantos);
}

static int temEpisodios(int alvo, const char *prefixo, int quantos) {
  int i;
  if (cat_n_episodios(alvo) != quantos) return 0;
  for (i = 0; i < quantos; i++) {
    char esperado[64];
    const CatEp *e = cat_episodio(alvo, i);
    snprintf(esperado, sizeof esperado, "%s-E%d", prefixo, i + 1);
    if (!e || strcmp(e->nome, esperado)) return 0;
  }
  return 1;
}

int main(void) {
  CatItem novo;
  CatItem lote[2];

  // A) UM TITULO ACRESCENTADO no fim nao apaga os episodios de quem ja estava.
  //    Este e o caso que a TV mostrava: a serie aberta perdia a secao inteira
  //    quando a descoberta trazia mais um titulo.
  montar();
  publicar(0, "A", 3);
  publicar(2, "C", 2);
  assert(temEpisodios(0, "A", 3));
  memset(&novo, 0, sizeof novo);
  snprintf(novo.titulo, sizeof novo.titulo, "novo");
  { int i = cat_acrescentar(&novo);
    assert(i == 3);
    assert(temEpisodios(0, "A", 3));
    assert(temEpisodios(2, "C", 2));
    // E o recem-chegado nasce VAZIO. realloc nao inicializa o que cresceu: um
    // epQtd de lixo aqui faria cat_episodio ler fora do vetor de episodios.
    assert(cat_n_episodios(i) == 0); }
  puts("ok  acrescentar um titulo preserva as faixas e o novo nasce vazio");

  // B) O MESMO PARA O LOTE, que e outro caminho de codigo.
  montar();
  publicar(1, "B", 4);
  memset(lote, 0, sizeof lote);
  snprintf(lote[0].titulo, sizeof lote[0].titulo, "L0");
  snprintf(lote[1].titulo, sizeof lote[1].titulo, "L1");
  { int idx[2];
    assert(cat_acrescentar_lote(lote, 2, idx) == 2);
    assert(temEpisodios(1, "B", 4));
    assert(cat_n_episodios(idx[0]) == 0 && cat_n_episodios(idx[1]) == 0); }
  puts("ok  acrescentar em lote preserva as faixas");

  // C) DOIS APPENDS SEGUIDOS. A cauda zerada da primeira vez nao pode ser
  //    zerada de novo por cima de episodios publicados depois dela.
  montar();
  { int i = cat_acrescentar(&novo);
    publicar(i, "D", 2);
    assert(cat_acrescentar(&novo) >= 0);
    assert(temEpisodios(i, "D", 2)); }
  puts("ok  publicar depois de crescer e crescer de novo mantem o que foi publicado");

  // D) TROCAR O CATALOGO INTEIRO INVALIDA, e tem de invalidar: os indices
  //    passam a apontar para outros titulos. E o oposto exato de (A), e por
  //    isso os dois estao no mesmo arquivo — quem mexer num vai ler o outro.
  montar();
  publicar(0, "A", 3);
  assert(temEpisodios(0, "A", 3));
  cat_definir_tudo(itens, 3, NULL, 0);
  assert(cat_n_episodios(0) == 0);
  puts("ok  trocar o catalogo inteiro invalida as faixas");

  puts("cateps: tudo ok");
  return 0;
}
