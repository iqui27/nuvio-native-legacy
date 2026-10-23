// CAPTURA DO PAINEL "Carregamento da Home" da ilha (ilha_atividade_detalhes +
// ilha_atividade_carga). Nao entra na suite: precisa de janela GL e de olho.
// Compila com -DANTES contra a ilha antiga (texto corrido) para o "antes".
#include "ilha.h"
#include "nvtmp.h"
#include "ilhasinais.h"
#include "dados.h"
#include "anim.h"
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

static SDL_Window *win;
static const char *base;
static GLuint fundoTex;
static unsigned ms; static int prontos, total = 5, fileiras, falhas, ativo = 1;
static const char *etapa = "Sincronizando a Home…";

static void fundo(void) {
  SDL_Surface *s = IMG_Load("deploy/app/art/00.jpg"), *t;
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
}
static void captura(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s; char cam[700]; int y;
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  for (y = 0; y < h; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", cam);
}
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(3);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    {
      char d[640];
#ifdef ANTES
      snprintf(d, sizeof d, "%s\nTempo: %u s · Add-ons consultados: %d de %d\nFileiras atualizadas: %d · Falhas: %d\nVocê pode continuar navegando enquanto a Home atualiza.", etapa, ms / 1000u, prontos, total, fileiras, falhas);
      ilha_atividade_detalhes("Carregamento da Home", d);
#else
      IlhaAtvCarga c = { NULL, ms, prontos, total, fileiras, falhas, ativo };
      ilha_atividade_detalhes("Carregamento da Home", etapa);
      ilha_atividade_carga(&c);
#endif
      ilha_atividade("Carregando fileiras… · Detalhes", -1.0f);
    }
    ilha_relogio_visivel(1);
    ilha_ancorar(96, 36, 0);
    ilha_desenhar(agora);
    ilhasinais_passo(agora);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}
int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700]; FILE *f; SDL_GLContext gl;
  base = argc > 1 ? argv[1] : nvt("/nuvio-homecarga-painel");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n"); fclose(f);
  ajustes_dir(dir);
  dados_iniciar("deploy/app");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: homecarga painel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar(1920, 1080);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_definir_vidro(1);
  fundo();
  quadros(60);
  ms = 11000; prontos = 3; fileiras = 0; falhas = 0;
  quadros(10);
  assert(ilha_modal_abrir());
  quadros(90);
  captura("1-carregando-3de5", 420);
  prontos = 5; fileiras = 14; falhas = 2; ms = 83000; quadros(70);
  captura("2-fileiras-falhas", 420);
  ativo = 0; etapa = "Home carregada"; falhas = 0; quadros(70);
  captura("3-concluida", 420);
  ilha_modal_fechar(0); quadros(40);
  SDL_Quit();
  return 0;
}
