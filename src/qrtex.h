#ifndef NV_QRTEX_H
#define NV_QRTEX_H
#include "qr.h"
#include "gfx.h"
#include <stdlib.h>
#include <string.h>

// Desenho compartilhado com o login: margem de quatro modulos e NEAREST.
static inline int qr_textura(GLuint *tex, const char *texto) {
  Qr q;
  int lado, x, y;
  unsigned char *px;
  if (!qr_gerar(&q, texto)) return 0;

  lado = q.lado + 2 * 4;
  px = (unsigned char *)malloc((size_t)lado * lado * 3);
  if (!px) return 0;
  memset(px, 255, (size_t)lado * lado * 3);   // fundo claro, inclusive a margem
  for (y = 0; y < q.lado; y++)
    for (x = 0; x < q.lado; x++)
      if (qr_modulo(&q, x, y)) {
        size_t i = ((size_t)(y + 4) * lado + (x + 4)) * 3;
        px[i] = px[i + 1] = px[i + 2] = 0;
      }

  if (!*tex) glGenTextures(1, tex);
  glBindTexture(GL_TEXTURE_2D, *tex);
  // NEAREST, nao LINEAR: um modulo borrado com o vizinho e o jeito mais rapido
  // de tornar o simbolo ilegivel numa camera de celular.
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, lado, lado, 0, GL_RGB, GL_UNSIGNED_BYTE, px);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  free(px);
  gfx_tex_esquecer(0);
  return 1;
}
#endif
