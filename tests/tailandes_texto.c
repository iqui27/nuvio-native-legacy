// #369 item 2: tailandes em todo caminho de texto do SDL_ttf, sem tofu. Rode com
// NUVIO_SEM_RESERVA_DE_SISTEMA=1 (so fontes embarcadas, como o wasm da Samsung e
// como a TV sem NotoSansThai em /system/fonts). Requer GL.
#include "gfx.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int falhas;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

int main(void) {
  TxtFamilia fam = TXT_FAMILIA_MONTSERRAT;
  char v[2048];
  int nc, falta;
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

  // "ภาษาไทย" (lingua tailandesa) e uma fala com latim, digitos e pontuacao.
  falta = txt_visual_da_linha(fam, TXT_DET_META, "ภาษาไทย", v, sizeof v, &nc);
  CHECK(falta == 0);
  falta = txt_visual_da_linha(fam, TXT_DET_SIN, "สวัสดีครับ ผมชื่อจอห์น Wick (2014) - ขอบคุณมาก…", v, sizeof v, &nc);
  CHECK(falta == 0);
  falta = txt_visual_da_linha(fam, TXT_DET_META, "2023 · แอ็คชั่น · ระทึกขวัญ", v, sizeof v, &nc);
  CHECK(falta == 0);
  // Controle: o latim continua sem reserva (nao pode ter regredido).
  falta = txt_visual_da_linha(fam, TXT_DET_META, "John Wick (2014)", v, sizeof v, &nc);
  CHECK(falta == 0);
  if (falhas) { printf("tailandes_texto: %d falha(s)\n", falhas); return 1; }
  puts("tailandes_texto: PASS (tailandes sem tofu so com fontes embarcadas)");
  return 0;
}
