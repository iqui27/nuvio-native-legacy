// Desenho das legendas de imagem (PGS) decodificadas: um quadro do imgsub vira
// texturas, uma por objeto, trocadas so quando o quadro muda.
#include "imgsub.h"
#include "legenda.h"
#include "gfx.h"
#include <string.h>

static unsigned id;
static GLuint tex[PGS_MAX_OBJ];

void imgsub_gl_limpar(void) {
  if (!id) return;
  glDeleteTextures(PGS_MAX_OBJ, tex);
  memset(tex, 0, sizeof tex);
  id = 0;
  gfx_tex_esquecer(0);
}

int imgsub_desenhar(double t, float x, float y, float w, float h, float alpha) {
  ImgsubQuadro *f;
  int i, ok = 0;
  if (!legenda_ligada_em(imgsub_dono_atual())) f = NULL;
  else f = imgsub_pegar(t);
  if (!f || f->n < 1) {
    imgsub_soltar(f);
    imgsub_gl_limpar();
    return 0;
  }
  if (f->id != id) {
    glDeleteTextures(PGS_MAX_OBJ, tex);
    memset(tex, 0, sizeof tex);
    for (i = 0; i < f->n; i++) {
      glGenTextures(1, &tex[i]);
      glBindTexture(GL_TEXTURE_2D, tex[i]);
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, f->obj[i].w, f->obj[i].h, 0,
                   GL_RGBA, GL_UNSIGNED_BYTE, f->obj[i].rgba);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    gfx_tex_esquecer(0);
    id = f->id;
  }
  for (i = 0; i < f->n && f->tela_w > 0 && f->tela_h > 0; i++) {
    GfxRect r = {x + w * f->obj[i].x / f->tela_w, y + h * f->obj[i].y / f->tela_h,
                 w * f->obj[i].w / f->tela_w, h * f->obj[i].h / f->tela_h};
    gfx_rect(r, tex[i], GFX_TEXTO, 0, 0, 0, 0, 1, 1, 1, alpha);
    ok = 1;
  }
  imgsub_soltar(f);
  return ok;
}
