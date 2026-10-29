// Spy for tests/textoescala_prova.sh. Linked into the shot programs.
//
// gfx.c is built with -Dgfx_rect=nvreal_gfx_rect and ajustes.c with
// -Dajustes_iniciar=nvreal_ajustes_iniciar -Dajustes_dir=nvreal_ajustes_dir, so every other file reaches the
// functions below. All text goes through gfx_rect(GFX_TEXTO), so one hook sees
// every text line, button and poster that a screen draws.
//
// NV_TXT_IDX=<n>  seeds "escalaTextoLocal n" in ajustes.txt before the settings
//                 load (old code ignores the key, which is the point).
// NV_TXT_LOG=<f>  "F n" opens captured frame n, then one line per draw, in
//                 REAL pixels (layout size * gfx_escala):
//                 "T h"      text line height
//                 "C t w h x y" GFX_CARD (poster art): texture, size, origin
//                 "R w h"    GFX_COR / GFX_ANEL under 150 px (buttons, pills)
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>

void nvreal_gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx,
                     float pary, float raio, float cr, float cg, float cb, float ca);
int nvreal_ajustes_iniciar(void);
void nvreal_ajustes_dir(const char *dir);

static FILE *log_, *frameLog_;
static unsigned frame_;
unsigned nvspy_quadros;   // cleared frames, for tests/textoescala_relogio.c

// Compare captured frames only; animation warm-up has variable draw counts.
void nvspy_glClear(GLbitfield mask) {
  if (frameLog_) fclose(frameLog_);
  frameLog_ = getenv("NV_TXT_LOG") ? tmpfile() : NULL;
  nvspy_quadros++;
  glClear(mask);
}
void nvspy_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h,
                       GLenum format, GLenum type, void *pixels) {
  glReadPixels(x, y, w, h, format, type, pixels);
  if (!log_ && getenv("NV_TXT_LOG")) log_ = fopen(getenv("NV_TXT_LOG"), "w");
  if (log_ && frameLog_) {
    char line[256];
    rewind(frameLog_);
    fprintf(log_, "F %u\n", frame_);
    while (fgets(line, sizeof line, frameLog_)) fputs(line, log_);
    fflush(log_);
  }
  frame_++;
}

void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float parx,
              float pary, float raio, float cr, float cg, float cb, float ca) {
  if (frameLog_) {
    const float e = gfx_escala(), w = r.w * e, h = r.h * e;
    if (modo == GFX_TEXTO) fprintf(frameLog_, "T %.2f\n", h);
    else if (modo == GFX_CARD) fprintf(frameLog_, "C %u %.1f %.1f %.1f %.1f\n", tex, w, h, r.x * e, r.y * e);
    else if ((modo == GFX_COR || modo == GFX_ANEL) && h >= 8 && h < 150 && w >= 8)
      fprintf(frameLog_, "R %.1f %.1f\n", w, h);
  }
  nvreal_gfx_rect(r, tex, modo, foco, parx, pary, raio, cr, cg, cb, ca);
}

static void semear(const char *dir) {
  const char *idx = getenv("NV_TXT_IDX");
  char caminho[800];
  FILE *f;
  if (!idx || !dir) return;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "a");
  if (f) { fprintf(f, "escalaTextoLocal %s\nescalaTextoVersaoLocal 1\n", idx); fclose(f); }
}

void ajustes_dir(const char *dir) {
  semear(dir);
  nvreal_ajustes_dir(dir);
}

// Settings scenes animate on the wall clock (ajc_onda); the AJUSTES_TESTE
// build has the real hook, the others link this no-op.
__attribute__((weak)) void ajustes_teste_cena_t(float t) { (void)t; }

// Some shots never call ajustes_dir; load the seeded file for them too.
int ajustes_iniciar(void) {
  if (getenv("NV_TXT_IDX") && getenv("NUVIO_DADOS")) ajustes_dir(getenv("NUVIO_DADOS"));
  ajustes_teste_cena_t(1.0f);   // scenes at a fixed instant: stable captures
  return nvreal_ajustes_iniciar();
}
