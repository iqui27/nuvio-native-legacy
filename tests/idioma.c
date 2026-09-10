// A tabela de traducao esta ORDENADA e responde? Sao as duas unicas maneiras
// de ela falhar: fora de ordem a busca binaria erra em silencio, e chave
// ausente devolve o portugues sem avisar.
//
//   cc tests/idioma.c src/idioma.c -Isrc -I/opt/homebrew/include \
//      -o /tmp/t-idioma && /tmp/t-idioma
// O -I do Homebrew e por causa do ajustes.h, que inclui SDL so pelo tipo de
// evento; nada de SDL e chamado por este teste.
#include "idioma.h"
#include <stdio.h>
#include <string.h>

// Dubles: o teste nao sobe ajustes.c inteiro so para saber o idioma.
static int ingles = 1;
int ajustes_idioma_ingles(void) { return ingles; }

static int falhas;
static void confere(const char *pt, const char *esperado) {
  const char *r = i18n(pt);
  if (strcmp(r, esperado) != 0) {
    printf("FALHOU: \"%s\" -> \"%s\" (esperava \"%s\")\n", pt, r, esperado);
    falhas++;
  }
}

int main(void) {
  // Uma de cada tela, incluindo as que tem acento no meio e aspas escapadas.
  confere("Continuar assistindo", "Continue Watching");
  confere("Ajustes", "Settings");
  confere("Quem está assistindo?", "Who's watching?");
  confere("Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.",
          "No direct source available. Use Reload to try again.");
  confere("Ficção científica", "Science Fiction");
  confere("Sáb", "Sat");
  confere("Mostrar \"Continuar assistindo\"", "Show \"Continue Watching\"");

  // Os selos da guia parental: nao sao literais no ponto de desenho (vem de
  // parental_rotulo/parental_gravidade), entao a varredura estatica nao os
  // cobre. Ficaram em portugues ate a v1.0.35 por isso.
  confere("Nudez", "Nudity");
  confere("Violência", "Violence");
  confere("Leve", "Mild");
  confere("Moderado", "Moderate");
  confere("Severo", "Severe");

  // Legendas montadas com %s: a frase pronta nunca casa com chave, entao a
  // parte em portugues tem de ser traduzida ANTES de entrar no formato.
  confere("Seleção do seu catálogo", "A selection from your catalog");
  confere("Conhecido por  %s", "Known for  %s");

  // Sem chave: devolve o proprio texto. E o caso de todo titulo de filme.
  confere("Blade Runner 2049", "Blade Runner 2049");
  confere("", "");

  // Em portugues NADA e traduzido, nem o que esta na tabela.
  ingles = 0;
  confere("Ajustes", "Ajustes");
  confere("Continuar assistindo", "Continuar assistindo");
  ingles = 1;

  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("idioma ok\n");
  return 0;
}
