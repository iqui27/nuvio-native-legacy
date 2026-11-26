// The chapter row and the seek strip (owner's sketch, 25/11), drawn by the
// real player with fixed thumbnails: the strip while scrubbing, the row
// opening, half-way through a slide and settled on the next chapter.
// Not in the suite (*_shot.sh): GL window.
//   bash tests/player_capitulos_shot.sh <dir>
#include "../src/seekr.h"
#include <SDL2/SDL_image.h>
#ifndef GL_UNPACK_ROW_LENGTH
#define GL_UNPACK_ROW_LENGTH 0x0CF2   /* GLES2 headers (Linux): Mesa takes it */
#endif
static GLuint artes[12];
static int estadoFix(void) { return SEEKR_PRONTO; }
static int chaveFix(void) { return 1; }
static void pedirFix(const char *id, int t, int e, long d) { (void)id; (void)t; (void)e; (void)d; }
// The strip: the frame at the scrub point and one either side.
static int quadrosFix(double pos, int n, GLuint *t, double *cue) {
  int i;
  for (i = 0; i < n; i++) {
    double c = n == 1 ? pos : pos + (i - 1) * 10.0;
    t[i] = artes[(n == 1 ? 1 : i + 0) % 12 + 8 > 11 ? 11 : (n == 1 ? 9 : 8 + i)];
    if (cue) cue[i] = c;
  }
  return 1;
}
static int velhoFix(void) { return 0; }
// Chapter i shows art i; the last one is "not made yet" (a plain tile).
static int nCapFix = 0;
static int capQuadrosFix(const int *idx, int n, GLuint *t) {
  int i;
  for (i = 0; i < n; i++) t[i] = idx[i] == nCapFix - 1 ? 0 : artes[idx[i] % 8];
  return 1;
}
static int capitulosLig(void) { return 1; }
#define seekr_estado estadoFix
#define seekr_tem_chave chaveFix
#define seekr_pedir pedirFix
#define seekr_quadros quadrosFix
#define seekr_quadro_velho velhoFix
#define seekr_capitulo_quadros capQuadrosFix
#define ajustes_capitulos capitulosLig
#include "../src/player.c"
#undef seekr_estado
#undef seekr_tem_chave
#undef seekr_pedir
#undef seekr_quadros
#undef seekr_quadro_velho
#undef seekr_capitulo_quadros
#undef ajustes_capitulos
#ifndef __APPLE__
// Linux (Mesa): the app's own context, GLES 2.0, not the macOS 2.1 one the
// glass fixture asks for.
static int attrShot(SDL_GLattr a, int v) {
  if (a == SDL_GL_CONTEXT_MAJOR_VERSION) { SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES); v = 2; }
  if (a == SDL_GL_CONTEXT_MINOR_VERSION) v = 0;
  return SDL_GL_SetAttribute(a, v);
}
#define SDL_GL_SetAttribute attrShot
// GLES reads back RGBA only: read that and pack it to the RGB the fixture saves.
static void lerRGB(GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, void *p) {
  unsigned char *q = malloc((size_t)w * h * 4), *o = p; size_t i;
  (void)f;
  glReadPixels(x, y, w, h, GL_RGBA, t, q);
  for (i = 0; i < (size_t)w * h; i++) { o[i * 3] = q[i * 4]; o[i * 3 + 1] = q[i * 4 + 1]; o[i * 3 + 2] = q[i * 4 + 2]; }
  free(q);
}
#define glReadPixels lerRGB
#endif
#define main iniciarFixtureGlass
#include "player_glass_shot.c"
#undef main
#undef SDL_GL_SetAttribute
#undef glReadPixels

static GLuint carregar(const char *p) {
  SDL_Surface *sf = IMG_Load(p), *t; GLuint tex = 0;
  if (!sf) return 0;
  t = SDL_ConvertSurfaceFormat(sf, SDL_PIXELFORMAT_ABGR8888, 0); SDL_FreeSurface(sf);
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
  return tex;
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  player_evento(&e);
  ultimoInput = relogio;   // the fixture's clock, not SDL's: the OSD stays up
}

int main(int argc, char **argv) {
  static const MkvCap caps[] = {
    { 0, "Cold open" }, { 102, "Opening" }, { 190, "The Harbor" }, { 665, "Night shift" },
    { 1188, "The deal" }, { 1650, "Crossing" }, { 2112, "Aftermath" }, { 2480, "End credits" } };
  char *init[] = { argv[0], argv[1], "capitulos-init" };
  int i;
  assert(argc == 2);
  assert(iniciarFixtureGlass(3, init) == 0);
  for (i = 0; i < 12; i++) { char p[64]; snprintf(p, sizeof p, "deploy/app/art/%02d.jpg", i + 2); artes[i] = carregar(p); }
  nCapFix = 8;
  ajustes_shot_valor("seekrLocal", 0);
  ajustes_shot_valor("seekrFitaLocal", 0);
  abrir(&filme); simular(1920, 1080, "", 0, 0);
  capmkv_aplicar(caps, 8);
  player_shot_estado(relogio, 1290.0f, 2580.0f, 1, 0, 1, 1);
  player_shot_buscando(1);
  quadros(40); salvar("1-strip");
  tecla(SDLK_UP);
  quadros(5); salvar("2-opening");
  quadros(40); salvar("3-chapters");
  tecla(SDLK_RIGHT);
  quadros(6); salvar("4-sliding");
  quadros(40); salvar("5-next");
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  quadros(40); salvar("6-not-made");
  printf("capSel %d capPos %.2f capAnim %.2f scrubbing %d\n", capSel, capPos, capAnim, scrubbing);
  tecla(SDLK_RETURN);
  quadros(40); salvar("7-after-ok");
  printf("after OK: posSeg %.1f capFoco %d scrubbing %d\n", posSeg, capFoco, scrubbing);
  // v6.1: at the last and first chapters the row reaches both margins.
  tecla(SDLK_UP); quadros(40);
  for (i = 0; i < 7; i++) { tecla(SDLK_RIGHT); quadros(3); }
  quadros(40); salvar("8-last");
  for (i = 0; i < 7; i++) { tecla(SDLK_LEFT); quadros(3); }
  quadros(40); salvar("9-first");
  printf("row centre at the first chapter: capCx %.0f\n", capCx);
  return 0;
}
