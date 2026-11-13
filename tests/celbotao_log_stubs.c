// Drawing/settings stubs for tests/celbotao_log.c: the diagnostics QR card's
// lifecycle runs headless (no window, no GL context, nothing focused). Only
// the symbols celbotao.c links against. GL calls track texture lifecycle.
#include <SDL2/SDL_opengl.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
int qr_switch;                 /* the "Diagnostics QR" setting under test */
const char *log_source;        /* registro_arquivo() under test */
int ajustes_diagnostics_qr(void) { return qr_switch; }
const char *registro_arquivo(void) { return log_source; }
const char *i18n(const char *s) { return s; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int ajustes_animacoes_reduzidas(void) { return 1; }
int ajustes_idioma(void) { return 0; }
int ajustes_tinta_foco(void) { return 255; }
int ajustes_vidro(void) { return 1; }
void ajustes_acento(float *r, float *g, float *b) { *r = *g = *b = 1; }
float gfx_escala(void) { return 1; }
float gfx_escala_ui(void) { return 1; }
float gfx_tex_aspect_atual;
int perftv_enabled, perftv_uploads;
#define NADA(f) void f(void) { abort(); }
NADA(gfx_cor) NADA(gfx_escala_entrar) NADA(gfx_escala_sair) NADA(gfx_icone) NADA(gfx_luz_canto)
NADA(gfx_rect) NADA(gfx_vidro_material) NADA(ponteiro_alvo) NADA(ponteiro_camada)
NADA(txt_bloco) NADA(txt_desenhar_alpha) NADA(txt_linha_corta)
int qr_uploads, qr_deletes, qr_allocations;
void glGenTextures(GLsizei n, GLuint *t) { assert(n == 1); *t = 42; qr_allocations++; }
void glDeleteTextures(GLsizei n, const GLuint *t) { assert(n == 1 && *t == 42); qr_deletes++; }
void glBindTexture(GLenum target, GLuint t) { assert(target == GL_TEXTURE_2D && t == 42); }
void glPixelStorei(GLenum p, GLint v) { assert(p == GL_UNPACK_ALIGNMENT && v == 1); }
void glTexParameteri(GLenum t, GLenum p, GLint v) { (void)t; (void)p; (void)v; }
void glTexImage2D(GLenum t, GLint level, GLint fmt, GLsizei w, GLsizei h, GLint border, GLenum ext, GLenum type, const GLvoid *px) {
  (void)t; (void)level; (void)fmt; (void)border; (void)ext; (void)type;
  assert(w == h && w > 8 && px);
  const unsigned char *rgb = px;
  for (int i = 0; i < w * 3 * 4; i++) assert(rgb[i] == 255);
  qr_uploads++;
}
int ponteiro_ativo(void) { return 0; }
