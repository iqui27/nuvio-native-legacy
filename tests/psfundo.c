// A lista de arte de fundo da tela de escolha de perfil (issue #90).
//
// O que este arquivo prova, e que so se prova sem TV:
//   - catalogo vazio -> lista vazia (a tela continua como era, preto liso)
//   - nenhum indice fora de cat_n(), em nenhuma das tres fontes
//   - a mesma semente na sessao da a MESMA ordem (sorteio estavel)
//   - a rotacao no tempo e uma funcao pura, inclusive quando SDL_GetTicks da
//     a volta em 2^32
//
// So src/psfundo.c + src/catalogo.c + src/artehero.c, com dubles para o resto:
// nada disto depende de SDL, de rede nem de disco (mesma razao de
// tests/catcache.sh).
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/psfundo.h"
#include "../src/catalogo.h"
#include "../src/progresso.h"

// --- DUBLES ------------------------------------------------------------------
// catalogo.c pede identidade e pasta gravavel para o cache em disco. Aqui nada
// e gravado: o catalogo entra por cat_definir_tudo, em memoria.
int ajustes_idioma_ingles(void) { return 0; }
const char *i18n(const char *s) { return s; }
const char *dados_dir(void)      { return ""; }
const char *sessao_usuario(void) { return ""; }
int         perfis_ativo(void)   { return 1; }
const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; return 0;
}

// --- APOIO -------------------------------------------------------------------
static void semear(int n, int nFileiras) {
  static CatItem itens[40];
  static CatFileira fils[4];
  int i;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < n && i < (int)(sizeof itens / sizeof *itens); i++) {
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Titulo %d", i);
    snprintf(itens[i].backdrop, sizeof itens[i].backdrop,
             "https://image.tmdb.org/t/p/w1280/art%d.jpg", i);
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%07d", i + 1);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "movie");
  }
  if (nFileiras >= 1) {
    snprintf(fils[0].chave, sizeof fils[0].chave, "addon_movie_top");
    fils[0].ini = 0; fils[0].n = n < 4 ? n : 4;
  }
  if (nFileiras >= 2) {
    snprintf(fils[1].chave, sizeof fils[1].chave, "addon_series_novos");
    fils[1].ini = n < 4 ? n : 4; fils[1].n = n > 4 ? n - 4 : 0;
  }
  cat_definir_tudo(itens, n, fils, nFileiras);
  psfundo_limpar();
}

static void confere(const char *o_que, int ok) {
  printf("%s  %s\n", ok ? "ok " : "FALHA", o_que);
  assert(ok);
}

int main(void) {
  int i;

  // --- CATALOGO VAZIO: a tela fica exatamente como era -----------------------
  semear(0, 0);
  psfundo_garantir("", 0);
  confere("catalogo vazio -> lista vazia", psfundo_n() == 0);
  confere("catalogo vazio -> sem url", psfundo_url(0) == NULL);
  confere("posicao negativa nao e indice", psfundo_indice(-1) == -1);

  // --- FONTE "" : os primeiros do catalogo ----------------------------------
  semear(25, 2);
  psfundo_garantir("", 0);
  confere("topo do catalogo enche a lista", psfundo_n() == PSFUNDO_MAX);
  for (i = 0; i < psfundo_n(); i++) confere("topo em ordem", psfundo_indice(i) == i);
  confere("url de arte existe", psfundo_url(0) != NULL);
  confere("url e a arte do item",
          !strcmp(psfundo_url(3), "https://image.tmdb.org/t/p/w1280/art3.jpg"));
  confere("pos fora da faixa nao devolve url", psfundo_url(PSFUNDO_MAX) == NULL);

  // Catalogo menor que a lista: sem indice inventado.
  semear(3, 1);
  psfundo_garantir("", 0);
  confere("catalogo de 3 -> lista de 3", psfundo_n() == 3);

  // --- FONTE "*" : sorteio ESTAVEL na sessao --------------------------------
  semear(25, 2);
  psfundo_garantir("*", 12345u);
  {
    int primeiro[PSFUNDO_MAX], n = psfundo_n();
    confere("sorteio enche a lista", n == PSFUNDO_MAX);
    for (i = 0; i < n; i++) primeiro[i] = psfundo_indice(i);
    // Mesma semente, mesma chamada: nada remonta e nada muda.
    psfundo_garantir("*", 12345u);
    for (i = 0; i < n; i++) confere("mesma semente, mesma ordem",
                                    psfundo_indice(i) == primeiro[i]);
    // Sem repeticao e dentro do catalogo — o Fisher-Yates parcial garante os
    // dois, e e justamente o que um sorteio ingenuo com % erra.
    for (i = 0; i < n; i++) {
      int j;
      confere("indice dentro do catalogo",
              psfundo_indice(i) >= 0 && psfundo_indice(i) < cat_n());
      for (j = i + 1; j < n; j++) confere("sem titulo repetido",
                                          psfundo_indice(i) != psfundo_indice(j));
    }
  }
  // Semente diferente sorteia outra coisa (senao "aleatorio" era enfeite).
  {
    int antes = psfundo_indice(0);
    int mudou = 0;
    psfundo_garantir("*", 999u);
    for (i = 0; i < psfundo_n(); i++) if (psfundo_indice(i) != antes) mudou = 1;
    confere("semente diferente muda a lista", mudou);
  }

  // --- FONTE = chave de fileira ---------------------------------------------
  semear(25, 2);
  psfundo_garantir("addon_movie_top", 0);
  confere("fileira escolhida da os itens dela", psfundo_n() == 4);
  for (i = 0; i < psfundo_n(); i++)
    confere("fileira: indices na janela dela", psfundo_indice(i) == i);

  // Chave que nao existe NAO e erro: cai no automatico, como na home.
  psfundo_garantir("addon_que_nao_existe", 0);
  confere("chave ausente cai no topo do catalogo", psfundo_n() == PSFUNDO_MAX);
  confere("chave ausente: topo em ordem", psfundo_indice(0) == 0);

  // O catalogo encolher nao pode deixar indice velho para tras.
  psfundo_garantir("addon_movie_top", 0);
  semear(2, 1);
  psfundo_garantir("addon_movie_top", 0);
  for (i = 0; i < psfundo_n(); i++)
    confere("catalogo menor: indice ainda valido",
            psfundo_indice(i) >= 0 && psfundo_indice(i) < cat_n());

  // --- ROTACAO NO TEMPO (funcao pura) ---------------------------------------
  confere("tempo: lista vazia fica em 0", psfundo_pos_no_tempo(9999, 0, 7000, 0) == 0);
  confere("tempo: intervalo 0 nao divide", psfundo_pos_no_tempo(9999, 0, 0, 5) == 0);
  confere("tempo: antes do primeiro passo", psfundo_pos_no_tempo(6999, 0, 7000, 5) == 0);
  confere("tempo: primeiro passo", psfundo_pos_no_tempo(7000, 0, 7000, 5) == 1);
  confere("tempo: quarto passo", psfundo_pos_no_tempo(28000, 0, 7000, 5) == 4);
  confere("tempo: da a volta na lista", psfundo_pos_no_tempo(35000, 0, 7000, 5) == 0);
  confere("tempo: inicio deslocado", psfundo_pos_no_tempo(10000, 3000, 7000, 5) == 1);
  // SDL_GetTicks passa de 2^32 depois de 49 dias de TV ligada. A subtracao em
  // unsigned continua certa por cima da volta; em int seria negativa.
  confere("tempo: sobrevive a volta do contador",
          psfundo_pos_no_tempo(1000u, 0xFFFFFFFFu - 6000u, 7000, 5) == 1);

  puts("psfundo: tudo ok");
  return 0;
}
