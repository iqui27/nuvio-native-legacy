// CAPTURA DA ILHA DO RELOGIO (ilha.h) sobre uma arte de destaque: relogio em
// repouso, a pilula no meio da mola, o toast da central ja aberto, um erro,
// uma atividade com barra e a ilha ancorada a direita (layout Dinamica).
//
// NAO ENTRA NA SUITE (*_shot.sh): precisa de janela GL e de olho humano. Inclui
// avisos.c como avisos_toast_shot, para acionar o toast de verdade.
#include "avisos.h"
#include "nvtmp.h"
#include "ilha.h"
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

#include "../src/avisos.c"

static SDL_Window *win;
static const char *base;

static void captura(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.bmp", base, nome);
  assert(SDL_SaveBMP(s, cam) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", cam);
}

// Roda `n` quadros; captura o ultimo se `nome`.
static void quadros(int n, const char *nome, int direita) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    GLuint fundo;
    avisos_atualizar(1.0f / 60.0f, agora);
    tex_novo_quadro();
    tex_bombear(3);
    glClearColor(0.05f, 0.05f, 0.055f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fundo = tex_obter("deploy/app/art/00.jpg");
    if (fundo) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundo, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    avisos_desenhar(agora);
    ilha_relogio_visivel(ajustes_relogio_ligado());
    if (direita == 2) ilha_posicionar(0);   // escolha de Ajustes (relogio na tela / posicao)
    else if (direita) ilha_ancorar(1920 - 64, 36, 1);
    ilha_desenhar(agora);
    if (i == n - 1 && nome) captura(nome);
    SDL_GL_SwapWindow(win);
    SDL_Delay(16);
  }
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char ajustes[700];
  SDL_GLContext gl;
  FILE *f;
  base = argc > 1 ? argv[1] : nvt("/nuvio-ilha");
  assert(dir && *dir);
  snprintf(ajustes, sizeof ajustes, "%s/ajustes.txt", dir);
  f = fopen(ajustes, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme %s\n", getenv("NUVIO_SHOT_THEME") ? getenv("NUVIO_SHOT_THEME") : "2");
  fclose(f);
  ajustes_dir(dir);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: ilha", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  quadros(60, "1-relogio", 0);
  toast(3);
  quadros(7, "2-morfando", 0);
  quadros(50, "3-aviso", 0);
  ilha_retirar("avisos");
  ilha_avisar("erro", ILHA_ERRO, NULL, "Não deu para falar com o servidor de recomendações", 4000, 0);
  quadros(60, "4-erro", 0);
  ilha_retirar("erro");
  { int i;
    for (i = 0; i < 70; i++) {
      ilha_atividade("Baixando a atualização...", 0.42f);
      quadros(1, i == 69 ? "5-atividade" : NULL, 0);
    } }
  quadros(60, NULL, 0);
  ilha_avisar("ok", ILHA_OK, NULL, "Atualizado. Feche e abra o app para usar.", 4000, 0);
  quadros(60, "6-direita", 1);

  // AJUSTES > Aparencia: Relogio na tela / Posicao do relogio. Rele o arquivo
  // (como o main) com cada combinacao; V_LIGA: 0 = Ligado, 1 = Desligado.
  ilha_retirar("ok");
  { const struct { int lig, pos; const char *nome; } c[] = {
      { 0, 1, "7-pos-esquerda" }, { 0, 2, "8-pos-direita" }, { 1, 1, "9-desligado-repouso" } };
    int i;
    for (i = 0; i < 3; i++) {
      f = fopen(ajustes, "w"); assert(f);
      fprintf(f, "idioma 0\nselected_theme 2\nrelogioTelaLocal %d\nrelogioPosLocal %d\n", c[i].lig, c[i].pos);
      fclose(f);
      ajustes_dir(dir);
      assert(ajustes_relogio_ligado() == !c[i].lig && ajustes_relogio_pos() == c[i].pos);
      quadros(80, c[i].nome, 2);
    }
    // Desligado, o AVISO ainda sai da pilula e some depois.
    ilha_avisar("ok2", ILHA_OK, NULL, "Atualizado. Feche e abra o app para usar.", 1500, 0);
    quadros(60, "10-desligado-com-aviso", 2);
    quadros(120, "11-desligado-aviso-passou", 2); }

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas da ilha gravadas.");
  return 0;
}
