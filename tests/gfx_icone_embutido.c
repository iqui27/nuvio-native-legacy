// TPK antigo + nucleo atualizado por .so: o celular nao depende do res/art
// antigo. Sem contexto GL; contamos upload/desenho/destruicao de verdade.
#include "gfx_icone_embutido_gl.h"
#include "gfx.h"
#include "gfx_smartphone_png.h"
#include "corviva.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int gerados, uploads, desenhos, apagados, arquivos, tentativas, falharAlocacao;
static GLuint ligado;
static float cor[4];
static unsigned char pixelsUpload[128 * 128 * 4];
int nv_grad_ativo;
float nv_acento_viva[3], nv_grad_viva[3][3], nv_tempo_viva;
float nv_ambiente_viva[4][3], nv_ambiente_forca;
int ajustes_profundidade(void) { return 0; }
float ajustes_profundidade_brilho(void) { return 0; }

void teste_glGenTextures(GLsizei n, GLuint *t) {
  tentativas += n;
  if (falharAlocacao) { while (n-- > 0) *t++ = 0; return; }
  while (n-- > 0) *t++ = (GLuint)++gerados;
}
void teste_glDeleteTextures(GLsizei n, const GLuint *t) {
  assert(n == 1 && t && *t > 0); apagados += n;
}
void teste_glBindTexture(GLenum target, GLuint t) { assert(target == GL_TEXTURE_2D); ligado = t; }
void teste_glTexImage2D(GLenum target, GLint level, GLint internalformat,
                      GLsizei w, GLsizei h, GLint border, GLenum format,
                      GLenum type, const void *pixels) {
  assert(target == GL_TEXTURE_2D && level == 0 && internalformat == GL_RGBA);
  assert(w == 128 && h == 128 && border == 0 && format == GL_RGBA && type == GL_UNSIGNED_BYTE);
  assert(ligado && pixels); memcpy(pixelsUpload, pixels, sizeof pixelsUpload); uploads++;
}
void teste_glTexParameteri(GLenum target, GLenum pname, GLint param) {
  assert(target == GL_TEXTURE_2D);
  if (pname == GL_TEXTURE_MIN_FILTER || pname == GL_TEXTURE_MAG_FILTER) assert(param == GL_LINEAR);
  else { assert(pname == GL_TEXTURE_WRAP_S || pname == GL_TEXTURE_WRAP_T); assert(param == GL_CLAMP_TO_EDGE); }
}
void teste_glPixelStorei(GLenum pname, GLint param) { assert(pname == GL_UNPACK_ALIGNMENT && param == 1); }
void teste_glDrawArrays(GLenum mode, GLint first, GLsizei count) {
  assert(mode == GL_TRIANGLE_STRIP && first == 0 && count == 4); desenhos++;
}
void teste_glUniform4f(GLint location, GLfloat a, GLfloat b, GLfloat c, GLfloat d) {
  (void)location; cor[0] = a; cor[1] = b; cor[2] = c; cor[3] = d;
}
GLuint tex_obter_larg(const char *cam, float w) {
  assert(w > 0); arquivos++;
  assert(strstr(cam, "/icones/"));
  return 0;  // pacote antigo: nao tem aj_smartphone.png
}

int main(void) {
  GfxRect r = { 10, 20, 28, 28 };
  unsigned char origem[sizeof gfx_smartphone_png + 1];
  FILE *f = fopen("deploy/app/art/icones/aj_smartphone.png", "rb");
  SDL_Surface *s, *rgba;
  int i;
  assert(f);
  assert(fread(origem, 1, sizeof origem, f) == sizeof gfx_smartphone_png);
  fclose(f);
  assert(!memcmp(origem, gfx_smartphone_png, sizeof gfx_smartphone_png));
  gfx_icones_dir("/pacote-antigo-sem-icone/art");
  assert(gerados == 0 && uploads == 0);  // nao paga GPU antes de usar
  gfx_icone(r, "aj_smartphone", 1, 1, 1, 0);
  gfx_icone((GfxRect){ 0, 0, 0, 0 }, "aj_smartphone", 1, 1, 1, 1);
  assert(gerados == 0 && uploads == 0 && arquivos == 0 && desenhos == 0);
  gfx_icone(r, "aj_smartphone", 0.8f, 0.7f, 0.6f, 1.0f);
  assert(desenhos == 1 && uploads == 1 && gerados == 1 && arquivos == 0);
  assert(cor[0] == 0.8f && cor[1] == 0.7f && cor[2] == 0.6f && cor[3] == 1.0f);
  s = IMG_Load("deploy/app/art/icones/aj_smartphone.png"); assert(s);
  rgba = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0); assert(rgba);
  assert(rgba->w == 128 && rgba->h == 128 && rgba->pitch == 128 * 4);
  assert(!memcmp(pixelsUpload, rgba->pixels, sizeof pixelsUpload));
  SDL_FreeSurface(s); SDL_FreeSurface(rgba);
  for (i = 0; i < 600; i++) gfx_icone(r, "aj_smartphone", 0.1f, 0.2f, 0.3f, 0.5f);
  assert(desenhos == 601 && uploads == 1 && gerados == 1 && arquivos == 0);
  assert(cor[0] == 0.1f && cor[1] == 0.2f && cor[2] == 0.3f && cor[3] == 0.5f);
  gfx_icone(r, "play", 1, 1, 1, 1);
  assert(arquivos == 1 && desenhos == 601);  // caminho normal continua no cache
  gfx_encerrar();
  assert(apagados == 1);
  gfx_encerrar(); assert(apagados == 1);
  gfx_icone(r, "aj_smartphone", 1, 1, 1, 1);
  assert(uploads == 2 && gerados == 2 && desenhos == 602);
  gfx_encerrar(); assert(apagados == 2);
  falharAlocacao = 1;
  for (i = 0; i < 600; i++) gfx_icone(r, "aj_smartphone", 1, 1, 1, 1);
  assert(tentativas == 3 && uploads == 2 && desenhos == 602 && arquivos == 1);
  gfx_encerrar(); assert(apagados == 2);
  falharAlocacao = 0;
  gfx_icone(r, "aj_smartphone", 1, 1, 1, 1);
  assert(tentativas == 4 && uploads == 3 && desenhos == 603);
  gfx_encerrar(); assert(apagados == 3);
  puts("gfx_icone_embutido: sem arte antiga, upload unico, 600 quadros sem disco, lifecycle ok");
  return 0;
}
