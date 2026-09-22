// CAPTURAS DA BUSCA, sem depender da resposta de rede.
//
// O teste existe para olhar a distancia de sofa: campo ativo, foco da grade,
// cursor e estado vazio depois de duas letras. A lista real e assincrona e fica
// para o teste manual do aparelho; aqui o importante e a casca da interacao.
#include "busca.h"
#include "buscasrec.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Window *janela;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  busca_evento(&e);
}

static void quadro(void) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(12);
  gfx_novo_quadro();
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  busca_desenhar(SDL_GetTicks());
  SDL_GL_SwapWindow(janela);
}

static void captura(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int i, y;
  assert(pix);
  for (i = 0; i < 45; i++) quadro();
  quadro();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4,
           1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-busca";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600];
  SDL_GLContext gl;
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: revisao da Busca", SDL_WINDOWPOS_CENTERED,
                           SDL_WINDOWPOS_CENTERED, 1920, 1080,
                           SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(96);
  gfx_icones_dir("deploy/app/art");
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  busca_iniciar();

  snprintf(nome, sizeof nome, "%s-vazio.bmp", saida);
  captura(nome);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-foco.bmp", saida);
  captura(nome);
  tecla(SDLK_a);
  tecla(SDLK_g);
  snprintf(nome, sizeof nome, "%s-digitado.bmp", saida);
  captura(nome);

  // COM HISTORICO: campo vazio mostra as pilulas no lugar do estado vazio.
  // Termos de tamanhos diferentes e um longo, para a quebra de linha e o
  // "Limpar" no fim aparecerem; registrados do mais antigo para o mais novo.
  { static const char *termos[] = {
      "up", "interestelar", "the office", "dune", "o senhor dos aneis",
      "breaking bad", "matrix", "fundacao", "stranger things", "cidade de deus" };
    int i;
    for (i = 0; i < 10; i++) buscasrec_registrar(termos[i]); }
  busca_iniciar();
  snprintf(nome, sizeof nome, "%s-recentes.bmp", saida);
  captura(nome);
  // Da tecla "a" (coluna 0) ate a ultima coluna e mais um: a ponte leva as
  // pilulas. Depois desce uma linha, para o foco cair no meio da lista.
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-recentes-foco.bmp", saida);
  captura(nome);
  tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-recentes-foco2.bmp", saida);
  captura(nome);
  // Ate o "Limpar": fim da ultima linha.
  { int i; tecla(SDLK_DOWN); for (i = 0; i < 10; i++) tecla(SDLK_RIGHT); }
  snprintf(nome, sizeof nome, "%s-recentes-limpar.bmp", saida);
  captura(nome);
  puts("PASS: capturas da Busca gravadas.");
  return 0;
}
