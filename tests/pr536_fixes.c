#include <assert.h>
#include <stdio.h>
#ifdef TEST_SHEET
#include "../src/faixas.c"
static VideoLegendaEstilo style;
static int touched, saved;
VideoLegendaEstilo *player_leg_estilo(void) { return &style; }
void player_leg_estilo_tocou(int fields) { touched |= fields; }
void player_leg_estilo_mudou(void) { saved++; }
static int assActive;
int player_leg_estilo_tocado(int field) { return (touched & field) != 0; }
int assrender_ativo(void) { return assActive; }
int assrender_texto_simples(void) { return !assActive; }
int main(void) {
  ctl = EST_COR_BORDA;
  estEscolher(2);
  assert(style.corBorda == 2 && saved == 1);
  assert(touched & PLR_LEG_COR_BORDA);
  estEscolher(2);
  assert(saved == 1);
  ctl = EST_FONTE;
  estEscolher(1);
  assert(touched & PLR_LEG_FONTE);
  ciclarEstilo(4);
  ciclarEstilo(6);
  ciclarEstilo(8);
  assert(touched & PLR_LEG_FUNDO && touched & PLR_LEG_BORDA && touched & PLR_LEG_NEGRITO);
  touched = 0;
  estGravar(5, style.posicao);
  assert(!(touched & PLR_LEG_POSICAO));
  estGravar(5, style.posicao + 1);
  assert(touched & PLR_LEG_POSICAO);
  /* Authored ASS: only untouched fields read "preserved", and every field
   * stays editable so the sheet can create the override Settings would. */
  assActive = 1;
  touched = PLR_LEG_COR_BORDA;
  assert(!estiloPreservadoAss(EST_COR_BORDA));
  assert(estiloPreservadoAss(4) && estiloPreservadoAss(EST_COR));
  assert(!estiloPreservadoAss(0) && !estiloPreservadoAss(3) && !estiloPreservadoAss(7));
  int fundo = style.fundo;
  ciclarEstilo(4);
  assert(style.fundo != fundo && (touched & PLR_LEG_FUNDO) && !estiloPreservadoAss(4));
  ctl = EST_COR;
  estEscolher(style.cor);
  assert(touched & PLR_LEG_COR);
  puts("PASS sheet authored-ASS override tracking");
}
#else
#include "../src/assrender.c"
int main(void) {
  ASS_Library *library = ass_library_init();
  assert(library);
  assRenderer = ass_renderer_init(library);
  assert(assRenderer);
  assTrack = ass_new_track(library);
  assert(assTrack);
  int index = ass_alloc_style(assTrack);
  ASS_Style *style = &assTrack->styles[index];
  style->FontName = strdup("Arial");
  style->BorderStyle = 1;
  style->BackColour = 0x1234567fu;
  style->OutlineColour = 0xabcdef00u;
  style->Shadow = 3;
  PlainAssStyle p = {.outlineRgb = 0xffff00,
    .overrides = PLAIN_ASS_OUTLINE | PLAIN_ASS_BORDER, .border = 2};
  ass_plain_style_locked(&p);
  assert(style->Shadow == 4 && style->BackColour == 0xffff007fu);
  assert(style->OutlineColour == 0xffff0000u);
  p.background = 2; p.overrides |= PLAIN_ASS_BG;
  ass_plain_style_locked(&p);
  assert(style->BorderStyle == 4 && style->BackColour == 174u);
  p.background = 0;
  ass_plain_style_locked(&p);
  assert(style->BorderStyle == 1 && style->BackColour == 0xffff002eu);
  p.overrides = 0;
  ass_plain_style_locked(&p);
  assert(style->BackColour == 0x1234567fu && style->Shadow == 3);
  assert(style->OutlineColour == 0xabcdef00u);
  ass_authored_styles_free();
  ass_free_track(assTrack);
  ass_renderer_done(assRenderer);
  ass_library_done(library);
  puts("PASS authored ASS shadow color, alpha, background and reset");
}
#endif
