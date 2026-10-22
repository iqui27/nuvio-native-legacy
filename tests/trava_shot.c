// CAPTURAS DA TRAVA DE SERIE: a modal em criacao, com perfis escolhidos, em
// Juntos e ja criada (gerenciar). Perfis e dados numa pasta descartavel, sem
// rede e sem conta.
//
// NAO ENTRA NA SUITE: precisa de janela GL e de olho humano.
//   NUVIO_SHOT_FONTE=3 bash tests/trava_shot.sh [prefixo]
#include "travatela.h"
#include "trava.h"
#include "perfis.h"
#include "ajustes.h"
#include "dados.h"
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

static void salvar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", cam);
}

static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(3); gfx_novo_quadro();
    travatela_atualizar(1.0f / 60.0f, agora);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    travatela_desenhar(agora);
    if (i == n - 1 && nome) salvar(nome);
    SDL_GL_SwapWindow(win); SDL_Delay(2);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  travatela_evento(&e);
  quadros(4, NULL);
}

static void carregarFundo(const char *cam) {
  SDL_Surface *s = IMG_Load(cam), *t;
  if (!s) return;
  t = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
  SDL_FreeSurface(s);
  if (!t) return;
  glGenTextures(1, &fundoTex);
  glBindTexture(GL_TEXTURE_2D, fundoTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, t->pitch / 4);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, t->w, t->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, t->pixels);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  SDL_FreeSurface(t);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS"), *tmp = getenv("NUVIO_TESTE_DIR");
  char cam[700];
  SDL_GLContext gl;
  FILE *f;
  int ids[2];
  base = argc > 1 ? argv[1] : "/tmp/nuvio-trava";
  if (!dir || !*dir || !tmp || strcmp(dir, tmp)) { fprintf(stderr, "trava_shot: rode por tests/trava_shot.sh\n"); return 1; }
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  // Quatro perfis da conta, o 1 ativo (formato do cache: perfis.txt).
  snprintf(cam, sizeof cam, "%s/perfis.txt", dir);
  f = fopen(cam, "w"); assert(f);
  fprintf(f, "1\t0\t1\t0\t#1E88E5\tHenrique\t\t\n2\t0\t0\t0\t#E91E63\tMaria\t\t\n"
             "3\t0\t0\t0\t#43A047\tLuca\t\t\n4\t0\t0\t0\t#FB8C00\tSala\t\t\n");
  fclose(f);
  snprintf(cam, sizeof cam, "%s/perfil.txt", dir);
  f = fopen(cam, "w"); assert(f); fputs("1\n", f); fclose(f);
  dados_iniciar("deploy/app");
  assert(!strcmp(dados_dir(), dir));
  ajustes_dir(dir);
  perfis_carregar_ativo();
  assert(perfis_n() == 4 && perfis_ativo() == 1);
  trava_dono("conta-shot");
  trava_iniciar();

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: Trava", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
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
  carregarFundo("deploy/app/art/00.jpg");

  // 1. CRIAR: so o perfil ativo marcado; a acao reclama de menos de 2.
  assert(!travatela_abrir("", "x"));
  assert(travatela_abrir("tt9000000", "Pluribus"));
  quadros(60, "1-nova");
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);   // modo
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);                                       // acao
  tecla(SDLK_RETURN);
  assert(travatela_aberto() && trava_n() == 0);                             // falta gente
  quadros(30, "2-aviso");

  // 2. ESCOLHER Maria e Luca, trocar para Juntos.
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);                                     // Maria
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);                                     // Luca
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RETURN);                   // modo -> Juntos
  quadros(40, "3-juntos");
  tecla(SDLK_RETURN);                                                       // volta Separados
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);                                     // PIN ligado
  quadros(40, "4-separados-pin");
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);                                     // criar
  assert(!travatela_aberto() && trava_n() == 1);
  assert(trava_da_serie("tt9000000", 1, ids, 2) == 1);
  { Trava t; assert(trava_obter(ids[0], &t) && t.n == 3 && t.pin == 1 && t.modo == TRAVA_SEPARADOS); }

  // 3. GERIR: reabrir mostra a trava; tirar um membro grava na hora.
  assert(travatela_abrir("tt9000000", "Pluribus"));
  quadros(60, "5-gerir");
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);                                     // tira Maria
  { Trava t; assert(trava_obter(ids[0], &t) && t.n == 2); }
  quadros(40, "6-gerir-menos");
  tecla(SDLK_ESCAPE);
  quadros(60, NULL);
  assert(!travatela_aberto());

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas da Trava gravadas.");
  return 0;
}
