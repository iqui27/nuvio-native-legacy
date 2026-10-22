// CAPTURAS DO DIARIO: Perfil e Stats SEM Trakt (alimentado pelo Diario de
// verdade, numa pasta descartavel), o estado vazio, e a ilha do "Pegou no
// sono" e da pergunta do cofre. Sem rede, sem conta, sem dado de pessoa: as
// artes sao do pacote.
//
// NAO ENTRA NA SUITE: precisa de janela GL e de olho humano.
//   NUVIO_SHOT_FONTE=3 bash tests/diario_shot.sh [prefixo]
#include "perfil.h"
#include "diario.h"
#include "diarioperfil.h"
#include "diarionuvem.h"
#include "dormiu.h"
#include "ajustes.h"
#include "dados.h"
#include "gfx.h"
#include "ilha.h"
#include "ilhasinais.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static SDL_Window *win;
static const char *base;
static GLuint fundoTex;

static void salvar(const char *nome, int h) {
  unsigned char *pix = malloc(1920 * (size_t)h * 4);
  SDL_Surface *s;
  char cam[700];
  int y;
  assert(pix);
  glReadPixels(0, 1080 - h, 1920, h, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, h, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < h; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(h - 1 - y) * 1920 * 4, 1920 * 4);
  snprintf(cam, sizeof cam, "%s-%s.png", base, nome);
  assert(IMG_SavePNG(s, cam) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", cam);
}

static void quadroPerfil(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro();
    perfil_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
    perfil_desenhar(SDL_GetTicks());
    if (i == n - 1 && nome) salvar(nome, 1080);
    SDL_GL_SwapWindow(win); SDL_Delay(2);
  }
}

static void quadroIlha(int n, const char *nome, int h) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(3);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (fundoTex) gfx_rect((GfxRect){ 0, 0, 1920, 1080 }, fundoTex, GFX_SNAP, 0, 0, 0, 0, 1, 1, 1, 1);
    ilha_relogio_visivel(1);
    ilha_ancorar(96, 36, 0);
    ilha_desenhar(agora);
    ilhasinais_passo(agora);
    if (i == n - 1 && nome) salvar(nome, h);
    SDL_GL_SwapWindow(win); SDL_Delay(16);
  }
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

static void vez(const char *id, int t, int e, long long q, int seg, char orig) {
  DiarioEntrada d; memset(&d, 0, sizeof d);
  snprintf(d.id, sizeof d.id, "%s", id); d.temporada = (short)t; d.episodio = (short)e;
  d.quando = q; d.seg = seg; d.durSeg = seg; d.origem = orig; d.hora = -1;
  if (orig == 'c') d.flags = DIARIO_F_DATA_APROX;
  diario_adicionar(1, &d);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS"), *tmp = getenv("NUVIO_TESTE_DIR");
  char cam[700];
  SDL_GLContext gl;
  PerfilDados pd;
  FILE *f;
  long long agora = (long long)time(NULL), dia = 86400;
  int i;
  base = argc > 1 ? argv[1] : "/tmp/nuvio-diario";
  if (!dir || !*dir || !tmp || strcmp(dir, tmp)) { fprintf(stderr, "diario_shot: rode por tests/diario_shot.sh\n"); return 1; }
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
  f = fopen(cam, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\n");
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  dados_iniciar("deploy/app");
  assert(!strcmp(dados_dir(), dir));
  ajustes_dir(dir);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("Nuvio: Diario", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
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
  diario_iniciar();

  // 1. ESTADO VAZIO sem Trakt: o que sobra quando o Diario ainda nao tem nada.
  assert(!diario_perfil_dados(1, &pd, agora));
  perfil_iniciar(); perfil_abrir();
  perfil_definir_dados(NULL);
  perfil_definir_estado(PERFIL_ESTADO_DESCONECTADO, "Seu Diário ainda está vazio.");
  quadroPerfil(90, "1-perfil-vazio");

  // 2. PERFIL DO DIARIO: um mes de player + conta, nomes e capas do pacote.
  { static const char *TIT[4] = { "Pluribus", "A Casa do Dragão", "Duna: Parte Dois", "Ruptura" };
    for (i = 0; i < 4; i++) {
      char id[16], poster[64];
      snprintf(id, sizeof id, "tt900000%d", i);
      snprintf(poster, sizeof poster, "deploy/app/art/poster/0%d.jpg", i);
      diario_nome_definir(id, TIT[i], 2020 + i, poster);
    }
    for (i = 0; i < 18; i++) {
      long long q = agora - (long long)(i * 37 % 26) * dia - 3600;
      if (i % 5 == 2) vez("tt9000002", 0, 0, q - 7 * 3600 * (i % 3), 9000, 'p');
      else vez(i % 3 ? "tt9000000" : "tt9000001", 1 + i % 2, 1 + i, q, 2700, i % 4 == 3 ? 'c' : 'p');
    }
    vez("tt9000003", 1, 1, agora - 2 * dia, 3100, 'p');
    vez("tt9000003", 1, 2, agora - dia, 3100, 'p'); }
  assert(diario_perfil_dados(1, &pd, agora));
  snprintf(pd.nome, sizeof pd.nome, "Perfil da Sala");
  perfil_definir_dados(&pd);
  quadroPerfil(90, "2-perfil-diario");

  // 3. ILHA: Pegou no sono (a pilula, e o modal aberto pela AZUL).
  carregarFundo("deploy/app/art/00.jpg");
  { DiarioEntrada e; memset(&e, 0, sizeof e);
    snprintf(e.id, sizeof e.id, "tt9000002"); e.dormiuSeg = 4320; e.durSeg = 9960;
    e.flags = DIARIO_F_DORMIU_SUS | DIARIO_F_PARCIAL; e.quando = agora;
    dormiu_perguntar("Duna: Parte Dois", "deploy/app/art/poster/02.jpg", "deploy/app/art/02.jpg", &e);
    quadroIlha(80, "3-dormiu-pilula", 200);
    { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_s;
      assert(ilha_evento(&ev)); }  // AZUL: o aviso abre o proprio modal
    quadroIlha(90, "4-dormiu-modal", 760);
    ilha_modal_fechar(1);
    while (ilha_aviso_vez()[0]) ilha_retirar(ilha_aviso_vez());
    quadroIlha(30, NULL, 0);
    // Episodio: a linha leva T/E.
    e.temporada = 1; e.episodio = 6; e.dormiuSeg = 1500; snprintf(e.id, sizeof e.id, "tt9000003");
    dormiu_perguntar("Ruptura", "deploy/app/art/poster/03.jpg", "deploy/app/art/03.jpg", &e);
    quadroIlha(40, NULL, 0);
    { SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_s;
      assert(ilha_evento(&ev)); }
    quadroIlha(90, "5-dormiu-modal-episodio", 760);
    ilha_modal_fechar(1);
    while (ilha_aviso_vez()[0]) ilha_retirar(ilha_aviso_vez());
    quadroIlha(30, NULL, 0); }

  // 4. ILHA: a pergunta do cofre (cresce sozinha).
  diarionuvem_mostrar_pergunta();
  quadroIlha(120, "6-cofre-pergunta", 760);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("PASS: capturas do Diario gravadas.");
  return 0;
}
