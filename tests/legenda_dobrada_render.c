/* Exercise the real primary renderer while native callbacks own fallback. */
#define faixas_legenda_overlay test_overlay
#define video_legenda_nativa test_native
#define legenda_cues test_cues
#include "../src/player.c"
#include <assert.h>

static int overlay, native_reads, collector_reads;
int faixas_legenda_overlay(void) { return overlay; }
int video_legenda_nativa(char *dst, int tam) {
  native_reads++;
  snprintf(dst, tam, "<i>Native fallback</i>");
  return 1;
}
int legenda_cues(double pos, int atraso, LegendaCue *out, int max) {
  (void)pos; (void)atraso; (void)max;
  collector_reads++;
  memset(out, 0, sizeof *out);
  snprintf(out->texto, sizeof out->texto, "Stale collector cue");
  return 1;
}

int main(void) {
  comVideo = 1;
  /* Both initial collection and a network retry can leave published cues. */
  for (int retry = 0; retry < 2; retry++) {
    float top = -1.f;
    desenharLegendaPrincipal(&top);
    assert(native_reads == retry + 1);
    assert(collector_reads == 0);
  }
  overlay = 1;
  float top = -1.f;
  desenharLegendaPrincipal(&top);
  assert(collector_reads == 1 && native_reads == 2);

  /* Fundo da legenda de duas linhas: UMA caixa para o bloco inteiro. Antes
   * cada linha desenhava a sua, com 7 px de sobreposicao no encontro — a faixa
   * dupla ficava mais escura. A caixa tambem precisa cobrir o vao ajustavel. */
  LegBloco bl = {0};
  bl.n = 2;
  bl.cor[0].w = 100.f; bl.cor[0].h = 30.f;
  bl.cor[1].w = 200.f; bl.cor[1].h = 30.f;
  legEstilo.espacamento = 5;
  bl.h = bl.cor[0].h + (float)video_leg_espaco(&legEstilo) + bl.cor[1].h;

  GfxRect caixa = caixaFundoBloco(&bl, 960.f, 500.f, 1);
  assert(caixa.y == 494.f && caixa.h == bl.h + 12.f);
  assert(caixa.x == 960.f - 100.f - 18.f && caixa.w == 200.f + 36.f);

  caixa = caixaFundoBloco(&bl, 100.f, 500.f, 0);
  assert(caixa.x == 100.f - 18.f && caixa.w == 200.f + 36.f);

  caixa = caixaFundoBloco(&bl, 1000.f, 500.f, 2);
  assert(caixa.x == 1000.f - 200.f - 18.f && caixa.w == 200.f + 36.f);

  /* O vao entra na caixa: sem isso o fundo fecharia entre as linhas quando a
   * pessoa aumenta o espacamento. */
  legEstilo.espacamento = 40;
  bl.h = bl.cor[0].h + (float)video_leg_espaco(&legEstilo) + bl.cor[1].h;
  caixa = caixaFundoBloco(&bl, 960.f, 500.f, 1);
  assert(caixa.h == 30.f + 40.f + 30.f + 12.f);

  puts("primary renderer: native fallback survives stale collector cues; handoff OK");
  puts("legenda dobrada: uma caixa de fundo por bloco, espacamento incluido");
}
