// Real modal transitions over Home glass: regional cost and material proof.
#include "ajustes.h"
#include "dados.h"
#include "recenviar.h"
#include "pessoas.h"
#include "salvos.h"
#include "salvospainel.h"
#include "text.h"
#include "tex_cache.h"
#include "vidro_ilhas.h"
#include <assert.h>

extern void ajustes_teste_vidro_env(void);
static int failures;
static const char *names[] = { "recenviar", "pessoas", "saved" };
static void update(int m, Uint32 t) {
  if (m == 0) recenviar_atualizar(1.0f / 60, t);
  if (m == 1) pessoas_atualizar(1.0f / 60, t);
  if (m == 2) spainel_atualizar(1.0f / 60, t);
}
static void draw(int m, Uint32 t) {
  if (m == 0) recenviar_desenhar(t);
  if (m == 1) pessoas_desenhar(t);
  if (m == 2) spainel_desenhar(t);
}
int main(void) {
  GLuint tex, fbo;
  CatItem item = {0};
  const char *dir = getenv("NUVIO_DADOS"), *out = getenv("NUVIO_FIX_PNGS");
  const char *phase = getenv("NUVIO_FIX_PHASE");
  int m, q;
  char path[800], name[100];
  assert(dir && out && phase);
  dados_iniciar(dir); ajustes_dir(dir);
  setenv("NUVIO_SHOT_VIDRO_FOSCO", "1", 1); ajustes_teste_vidro_env();
  setenv("NUVIO_ILHA_BARRA", "1", 1);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("frost regression", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); vidroFundoPreparar(); salvos_iniciar();
  snprintf(item.imdb, sizeof item.imdb, "tt12637874");
  snprintf(item.titulo, sizeof item.titulo, "Fallout");
  for (m = 0; m < 3; m++) {
    int bad = 0;
    double maxCopy = 0, maxFill = 0;
    if (m == 0) assert(recenviar_abrir(&item));
    if (m == 1) assert(pessoas_abrir());
    if (m == 2) spainel_abrir();
    for (q = 0; q < 110; q++) {
      Uint32 t = 100000 + m * 2000 + q * 16;
      double copy, fill;
      if (q == 75) {
        SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
        if (m == 0) recenviar_evento(&e);
        if (m == 1) pessoas_evento(&e);
        if (m == 2) spainel_fechar();
      }
      txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
      update(m, t); ilhaCena();
      // Changing Home foreground invalidates modal reuse even after settling.
      gfx_cor((GfxRect){450, 200, 1000, 700}, 0, .03f * (q % 4), 0, 0, .1f);
      copy = gfx_vidro_copia; fill = gfx_vidro_fill;
      draw(m, t);
      copy = gfx_vidro_copia - copy; fill = gfx_vidro_fill - fill;
      if (copy > maxCopy) maxCopy = copy;
      if (fill > maxFill) maxFill = fill;
      // Includes the 68px lens reach and filter padding. No full-screen work.
      if (copy >= .85 || fill >= .16) bad++;
      if (q == 70) {
        snprintf(name, sizeof name, "%s-%s", phase, names[m]); ilhaPng(out, name);
        if (getenv("NUVIO_FIX_MATERIAL")) {
          int x = m == 0 ? 1470 : m == 1 ? 1445 : 1855;
          failures += !ilhaConfere(names[m], x, 440, 20, 200);
          failures += gfx_vidro_com_fonte < 2;
        }
      }
    }
    printf("%s budget: %s copy=%.4f fill=%.4f frames=%d\n", names[m], bad ? "FAIL" : "PASS", maxCopy, maxFill, bad);
    failures += bad != 0;
  }
  if (getenv("NUVIO_FIX_MATERIAL")) {
    gfx_novo_quadro(); glClearColor(.1f, .1f, .1f, 1); glClear(GL_COLOR_BUFFER_BIT);
    // Even a stale/nonzero source counter cannot make a solid surface pass.
    gfx_vidro_com_fonte = 2;
    if (ilhaConfere("opaque negative control", 800, 440, 20, 200)) failures++;
    ilhaCena();
    gfx_vidro_com_fonte = 2;
    if (ilhaConfere("sharp negative control", 800, 440, 120, 200)) failures++;
  }
  snprintf(path, sizeof path, "%s/%s-result.txt", out, phase);
  FILE *f = fopen(path, "w"); assert(f); fprintf(f, "failures=%d\n", failures); fclose(f);
  return failures ? 1 : 0;
}
