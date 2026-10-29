// Real detail input and OpenGL rendering, with offline comment data only.
#include <assert.h>
#include "dados.h"
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#define extras_n_comentarios fixture_count
#define extras_n_comentarios_ep fixture_count
#define extras_comentario_usuario fixture_author
#define extras_comentario_ep_usuario fixture_author
#define extras_comentario_texto fixture_text
#define extras_comentario_ep_texto fixture_text
#define extras_comentario_spoiler fixture_spoiler
#define extras_comentario_data fixture_date
#define extras_comentario_respostas fixture_replies
#define extras_comentario_nota fixture_rating
#define extras_comentario_ep_nota fixture_rating
#define extras_comentario_curtidas fixture_likes
#define extras_comentario_ep_curtidas fixture_likes
#define txt_linha capture_line
#include "../src/detail.c"
#undef NV_TXT_TELA
#include "../src/sinopse.c"
#undef txt_linha
TxtLinha txt_linha(TxtEstilo, const char *, int, int, int, int);
static int close_draws;
int fixture_spoiler(int ep, int i) { (void)ep; return i == 2; }
const char *fixture_date(int ep, int i) { (void)ep; (void)i; return "2026-10-12T12:00:00Z"; }
int fixture_replies(int ep, int i) { (void)ep; return i + 2; }
int fixture_rating(int i) { return 9 - i; }
int fixture_likes(int i) { return 42 - i * 5; }
static char long_text[18000];
int fixture_count(void) { return 3; }
const char *fixture_author(int i) { return i == 0 ? "Avery" : i == 1 ? "Morgan" : "Sam"; }
const char *fixture_text(int i) {
  return i == 0 ? "A beautifully observed story, with quiet performances and a final scene that stays with you. Every detail rewards a second viewing." : i == 1 ? long_text : "The final reveal changes everything we thought we knew about the first episode.";
}
TxtLinha capture_line(TxtEstilo s, const char *v, int r, int g, int b, int a) {
  if (!strcmp(v, i18n("Fechar"))) close_draws++;
  return txt_linha(s, v, r, g, b, a);
}
static void key(SDL_Keycode k) {
  SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = k;
  detail_evento(&e); e.type = SDL_KEYUP; detail_evento(&e);
}
static void draw(void) {
  for (int j = 0; j < 24; j++) {
    txt_novo_quadro(); gfx_novo_quadro(); glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    tex_bombear(3);
  }
}
static void shot(const char *name) {
  char path[256]; snprintf(path, sizeof path, "/Volumes/ExternalSSD/tmp/rev/cominteiro/%s.png", name);
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(surface);
  for (int y = 0; y < 1080; y++) memcpy((char *)surface->pixels + y * surface->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(surface, path) == 0); SDL_FreeSurface(surface); free(pixels);
}
int main(void) {
  for (int j = 0; j < 70; j++) strcat(long_text, "A patient camera follows the characters through ordinary rooms, letting silence carry the story. The performances reveal something new each time.\n\n");
  strcat(long_text, "END OF REVIEW");
  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Review reader", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w); SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  char art[1024]; assert(realpath("deploy/app/art", art));
  gfx_icones_dir(art); extras_carregar(art);
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(32); gfx_borrao_iniciar(480, 270);
  assert(home_iniciar("deploy/app/art"));
  CatItem title = {0}; strcpy(title.titulo, "A story worth discussing"); strcpy(title.tipo, "series");
  cat_definir_tudo(&title, 1, NULL, 0); idx = 0;
  aberto = 1; saindo = 0; t = 1; nivel = 1; carro = 0;
  foco.fileira = SEC_COMENTARIOS; foco.coluna = nPilulasCom();
  SDL_Delay(2); key(SDLK_RETURN); draw();
  if (!sinopse_aberta()) { fprintf(stderr, "FAIL: OK on comment card did not open full-review reader\n"); return 1; }
  assert(sinopse_comentario_aberto() && !strcmp(leitor.texto, fixture_text(0)));
  shot("short-review");
  key(SDLK_RIGHT); draw();
  assert(comLeitorIndice == 1 && leitor.nLinhas > 6 && !strcmp(leitor.texto, long_text));
  int lines = leitor.nLinhas; void *cache = leitor.linhas;
  key(SDLK_DOWN); assert(leitor.destino > 0 && leitor.scroll < leitor.destino);
  key(SDLK_END); leitor.scroll = leitor.destino; draw();
  assert(leitor.max > 0 && leitor.scroll == leitor.max && leitor.nLinhas == lines && leitor.linhas == cache);
  assert(strstr(leitor.texto + leitor.linhas[lines - 1].inicio, "END OF REVIEW"));
  shot("long-review-scrolled");
  key(SDLK_RIGHT); draw(); assert(comLeitorIndice == 2 && leitor.oculto && !leitor.nLinhas);
  shot("spoiler-hidden");
  key(SDLK_RETURN); draw(); assert(!leitor.oculto && leitor.nLinhas > 0);
  key(SDLK_LEFT); draw(); assert(comLeitorIndice == 1 && leitor.scroll == 0);
  key(SDLK_ESCAPE);
  assert(!sinopse_aberta() && aberto && !saindo && nivel == 1 && foco.fileira == SEC_COMENTARIOS && foco.coluna == nPilulasCom());
  comentEp = 1; foco.coluna++; key(SDLK_RETURN); draw();
  assert(sinopse_comentario_aberto() && !strcmp(leitor.texto, long_text));
  key(SDLK_ESCAPE); assert(!sinopse_aberta() && foco.coluna == nPilulasCom() + 1);
  key(SDLK_RETURN); draw(); sinopse_fechar();
  okDesceEm = SDL_GetTicks() - NV_HOLD_MS - 1;
  SDL_Event release = {.type = SDL_KEYUP}; release.key.keysym.sym = SDLK_RETURN; detail_evento(&release);
  assert(!sinopse_aberta());
  for (int resolution = 0; resolution < 2; resolution++) {
    int width = resolution ? 3840 : 1280, height = resolution ? 2160 : 720;
    glViewport(0, 0, width, height); gfx_tamanho_alvo(width, height);
    key(SDLK_RETURN); draw();
    assert(sinopse_comentario_aberto() && !strcmp(leitor.texto, long_text) && leitor.nLinhas > 6);
    key(SDLK_ESCAPE); assert(!sinopse_aberta());
  }
  puts("PASS: show/episode OK opens full text, Back restores focus, Left/Right switches, spoiler reveal, cached scrolling, long press preserved");
  tex_encerrar(); txt_encerrar(); SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  return 0;
}
