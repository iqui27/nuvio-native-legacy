// #205: arabe sai na face Naskh Bold em todo peso (UI) e na legenda simples.
// UI: largura do arabe em peso comum == largura com negrito pedido (ambos na
// face Bold real). Legenda: plainass_document marca \b1 quando a Bold chegou.
#include "gfx.h"
#include "text.h"
#include "plainass.h"
#include "legenda.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  TxtFamilia fam = TXT_FAMILIA_MONTSERRAT;
  const char *ar = "كيانو ريفز في فيلم جديد";
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("t", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  txt_definir_fonte_interface(fam);
  txt_novo_quadro();
  { TxtLinha n = txt_linha_corta_enfase(TXT_DET_META, ar, 255, 255, 255, 255, 2000, fam, 0);
    TxtLinha b = txt_linha_corta_enfase(TXT_DET_META, ar, 255, 255, 255, 255, 2000, fam, TXT_ENF_NEGRITO);
    printf("w normal=%d negrito=%d\n", n.w, b.w);
    CHECK(n.w > 0 && n.w == b.w); }
  plainass_arabic_bold = 1;
  { LegendaCue c = {0}; c.inicio = 1; c.fim = 2; snprintf(c.texto, sizeof c.texto, "%s", ar);
    char *d = plainass_document(&c, 1);
    CHECK(d && strstr(d, "\\fnNoto Naskh Arabic\\b1")); free(d); }
  plainass_arabic_bold = 0;
  { LegendaCue c = {0}; c.inicio = 1; c.fim = 2; snprintf(c.texto, sizeof c.texto, "%s", ar);
    char *d = plainass_document(&c, 1);
    CHECK(d && !strstr(d, "\\b1")); free(d); }
  printf(falhas ? "FALHOU\n" : "OK\n");
  return falhas != 0;
}
