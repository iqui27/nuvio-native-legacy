// Exercise the actual style application and libass rasterizer with Regular only.
#define main previous_area_main
#include "ass_area.c"
#undef main
#include "../src/assrender.c"
#include <assert.h>
int perftv_enabled, perftv_uploads;

static unsigned long render_ink(void) {
  unsigned long ink = 0;
  ASS_Image *im = ass_render_frame(assRenderer, assTrack, 1500, NULL);
  for (; im; im = im->next)
    if (im->type == IMAGE_TYPE_CHARACTER)
      for (int y = 0; y < im->h; y++)
        for (int x = 0; x < im->w; x++) ink += im->bitmap[y * im->stride + x];
  return ink;
}
int main(void) {
  // Render synchronously under test; do not start the production worker.
  assWorkerCriado = 1;
  assLib = ass_library_init(); assert(assLib);
  size_t n;
  char *font = ler("deploy/app/fonts/NotoNaskhArabic-Regular.ttf", &n); assert(font);
  ass_add_font(assLib, "NotoNaskhArabic-Regular.ttf", font, n); free(font);
  assRenderer = ass_renderer_init(assLib); assert(assRenderer);
  ass_set_frame_size(assRenderer, 1920, 1080);
  ass_set_fonts(assRenderer, NULL, "Noto Naskh Arabic", ASS_FONTPROVIDER_NONE, NULL, 1);
  plainass_arabic_bold = 0; assNaskhBold = 0;
  LegendaCue cue = {0}; cue.inicio = 1; cue.fim = 3;
  snprintf(cue.texto, sizeof cue.texto, "مرحبا بالعالم");
  for (int plain = 0; plain <= 1; plain++) {
    char *doc = plainass_document(&cue, 1); assert(doc);
    assUsaSistema = 1;
    assert(ass_carregar(doc, strlen(doc), 0, 0, plain)); free(doc);
    PlainAssStyle p = {0};
    snprintf(p.font, sizeof p.font, "Noto Naskh Arabic");
    p.size = 48; p.rgb = 0xffffff; p.overrides = PLAIN_ASS_BOLD;
    ass_plain_style_locked(&p);
    unsigned long regular = render_ink();
    p.bold = 1; ass_plain_style_locked(&p);
    unsigned long bold = render_ink();
    fprintf(stderr, "libass plain=%d: regular ink=%lu bold ink=%lu\n", plain, regular, bold);
    assert(regular > 0 && bold > regular);
    p.bold = 0; ass_plain_style_locked(&p); assert(render_ink() == regular);
    ass_authored_styles_free(); ass_free_track(assTrack); assTrack = NULL;
    ass_renderer_done(assRenderer);
    assRenderer = ass_renderer_init(assLib); assert(assRenderer);
    ass_set_frame_size(assRenderer, 1920, 1080);
    ass_set_fonts(assRenderer, NULL, "Noto Naskh Arabic", ASS_FONTPROVIDER_NONE, NULL, 1);
  }
  return 0;
}
