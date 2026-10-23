// #204: "Fonte do trailer" = YouTube na LG deixava a TV sem trailer nenhum.
//
// O log da 1.6.5 do relato: "[trailer] detalhe: sem trailer (ajuste 3, apple
// tem, imdb tem, youtube n/a)" e "[trailer] hero: sem fonte ... (ajuste 3)".
// O YouTube so toca no .wgt da Samsung (trailerfonte.c, existe); fora dele o
// valor 3 era escolhivel e zerava a lista de fontes: sem trailer no destaque e
// sem o botao Trailer na pagina do titulo.
//
// Fora do .wgt a linha tem 3 valores: o 3 gravado por uma versao anterior le
// como fora da faixa e fica Automatico; a seta nunca chega nele.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

static void escrever(const char *caminho, const char *texto) {
  FILE *f = fopen(caminho, "w");
  assert(f);
  fputs(texto, f);
  fclose(f);
}

int main(void) {
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-aj-trailerfonte-XXXXXX");
  char caminho[700];
  assert(mkdtemp(dir));
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  assert(nValores(AJ_TRAILER_FONTE) == 3);

  // O arquivo do relato: YouTube gravado na 1.6.3.
  escrever(caminho, "trailerFonteLocal 3\n");
  valor[AJ_TRAILER_FONTE] = 0;
  ajustes_dir(dir);
  assert(valor[AJ_TRAILER_FONTE] == 0);
  assert(ajustes_trailer_fonte() == 0);

  // As fontes que existem aqui continuam valendo.
  escrever(caminho, "trailerFonteLocal 2\n");
  ajustes_dir(dir);
  assert(valor[AJ_TRAILER_FONTE] == 2 && ajustes_trailer_fonte() == 2);
  escrever(caminho, "trailerFonteLocal 1\n");
  ajustes_dir(dir);
  assert(valor[AJ_TRAILER_FONTE] == 1 && ajustes_trailer_fonte() == 1);

  // Abrir a tela de Ajustes nao zera a escolha valida.
  ajustes_iniciar();
  assert(valor[AJ_TRAILER_FONTE] == 1);

  // A seta passa de IMDb para Automatico, sem parar no YouTube.
  valor[AJ_TRAILER_FONTE] = 2;
  assert(passoAdiante(AJ_TRAILER_FONTE, 1) == 0);
  valor[AJ_TRAILER_FONTE] = 0;
  assert(passoAdiante(AJ_TRAILER_FONTE, -1) == 2);

  // Mesmo que o valor[] receba 3 por fora, a fonte lida e Automatico.
  valor[AJ_TRAILER_FONTE] = 3;
  assert(ajustes_trailer_fonte() == 0);

  // VETO INVISIVEL (herotrailer, 2.0.5): a linha "Trailer no destaque" fica
  // Ligada e a home nao toca nada — SEGURO ou a GPU vetada tiram o efetivo sem
  // mexer no gravado. ajustes_trailer_hero_vetado() diz qual e o veto para a
  // ajuda da linha e para o log do portao.
  valor[AJ_HERO_TRAILER] = 0;              // Ligado (V_LIGA: 0 = ligado)
  valor[AJ_FOCO_TRAILER] = AJ_TRAILER_CARTAZ_DESLIGADO;
  assert(ajustes_trailer_hero() == 1);
  assert(ajustes_trailer_hero_vetado() == 0);
  ajustes_trailer_hero_vetar("Mali-450");
  assert(ajustes_trailer_hero() == 0);
  assert(ajustes_trailer_hero_vetado() == 2);
  // Desligado de verdade: o veto nao conta (a linha diz Desligado, pronto).
  valor[AJ_HERO_TRAILER] = 1;
  assert(ajustes_trailer_hero_vetado() == 0);
  // O cartaz em foco ligado sozinho tambem acusa o veto.
  valor[AJ_FOCO_TRAILER] = AJ_TRAILER_CARTAZ_CHEIA;
  assert(ajustes_trailer_hero_vetado() == 2);
  valor[AJ_FOCO_TRAILER] = AJ_TRAILER_CARTAZ_DESLIGADO;

  unlink(caminho);
  rmdir(dir);
  printf("ajustes_trailer_fonte: ok\n");
  return 0;
}
