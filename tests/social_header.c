// Exercise the real sheet renderer and D-pad without adding production test APIs.
#include "text.h"
#include <stdio.h>
static char title[200];
static GLuint titleTexture;
static float titleY;
static TxtLinha headerLine(TxtEstilo style, const char *text, int r, int g, int b, int a, float width) {
  TxtLinha line = txt_linha_corta(style, text, r, g, b, a, width);
  if (style == TXT_ILHA_TITULO) {
    snprintf(title, sizeof title, "%s", text);
    titleTexture = line.tex;
  }
  return line;
}
static void headerDraw(TxtLinha line, float x, float y, float alpha) {
  if (line.tex == titleTexture && alpha > .99f) titleY = y;
  txt_desenhar_alpha(line, x, y, alpha);
}
#define txt_linha_corta headerLine
#define txt_desenhar_alpha headerDraw
#include "../src/salvospainel.c"
#undef txt_linha_corta
#undef txt_desenhar_alpha
#include <SDL2/SDL_image.h>
#include <assert.h>
#include "j4_proof.h"

static int failures;
static void check(int ok, const char *what) {
  if (!ok) { fprintf(stderr, "FAIL: %s (tab=%d title=%s glass=%d)\n", what, aba, title, ajustes_vidro()); failures++; }
}
static void frame(float dt) {
  spainel_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  glClearColor(.12f, .16f, .22f, 1); glClear(GL_COLOR_BUFFER_BIT);
  spainel_desenhar(SDL_GetTicks());
}
static void key(SDL_Keycode key) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = key; spainel_evento(&e);
  e.type = SDL_KEYUP; spainel_evento(&e);
}
int main(int argc, char **argv) {
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_dir(dados_dir());
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Social header", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  GLuint texture, target;
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  char art[1024]; assert(realpath("deploy/app/art", art)); gfx_icones_dir(art);
  const char *labels[] = {"Salvos", "Atividade", "Amigos", "Avisos", "Agenda"};
  for (int lang = 0; lang < 2; lang++) for (int glass = 0; glass < 2; glass++) {
    char settings[256];
    snprintf(settings, sizeof settings, "idioma %d\nidiomaAutoLocal 0\nperformanceAutoVersaoLocal 1\nfonteInterface 3\n", lang);
    assert(dados_gravar("ajustes.txt", settings)); ajustes_dir(dados_dir()); ajustes_definir_vidro(glass);
    gfx_escala_ui_definir(1); gfx_escala_txt_definir(1);
    spainel_abrir();
    for (int i = 0; i < 100; i++) frame(.01f);
    for (int t = 0; t < SP_ABA_N; t++) {
      if (t) {
        char previous[200]; snprintf(previous, sizeof previous, "%s", i18n(labels[t - 1]));
        key(SDLK_RIGHT);
        check(aba == t && trocaT == 0, "D-pad switches tab and starts transition");
        check(!strcmp(tituloAnt, previous), "transition retains previous localized title");
        frame(.01f);
        check(trocaT > 0 && trocaT < 1, "transition remains animated");
      }
      for (int i = 0; i < 100; i++) frame(.01f);
      check(!strcmp(title, i18n(labels[t])), "rendered title equals selected localized tab label");
      check(SP_ABAS_Y >= SP_Y && SP_ABAS_Y + SP_ABAS_H < SP_KICK_Y && SP_KICK_Y < titleY,
            "tab row is above kicker and rendered title");
      check(SP_OPC_Y > titleY + txt_linha(TXT_ILHA_TITULO, title, 255, 255, 255, 255).h,
            "toolbar stays below title");
      if (lang == 1 && (t == 1 || t == 2)) {
        char path[1024]; snprintf(path, sizeof path, "%s/%s-glass%d.png", argv[1], t == 1 ? "activity" : "friends", glass);
        j4_save(path);
      }
    }
    editando = 1; frame(.01f);
    check(!strcmp(title, i18n("Editar abas")), "tab editor retains its localized title");
    editando = 0; spainel_fechar();
    for (int i = 0; i < 100; i++) frame(.01f);
  }
  printf("%s: Social header, five tabs, two languages, glass on/off, animated D-pad transitions\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
