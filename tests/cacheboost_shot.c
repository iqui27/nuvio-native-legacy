// F07 captures: the AUDIO sheet with the volume row (100%, boosted 150% in
// red, passthrough cap, not available on this TV), interface font Montserrat.
//   bash tests/cacheboost_shot.sh /Volumes/ExternalSSD/nv-f07-shots
// Derived from tests/legendas_shot.c. Original header:
// Captures of the F04 subtitle selector and of the two subtitles on screen
// (legendasui.c + legenda2.c through the real player draw path). GL window,
// not part of the suite; the pixel checks below keep it honest:
//
//   bash tests/legendas_shot.sh /Volumes/ExternalSSD/nv-f04-shots
//
// Frames: simple view, More options, dual subtitles without and with the
// controls, and a three-line primary to show the bands do not collide.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "plrilha.h"
#include "video.h"
#include "addons.h"
#include "legenda.h"
#include "legenda2.h"
#include "legendasui.h"
#include "linguas.h"
#include "cacheboost.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;
static const char *saida;
static unsigned char quadro[1920 * 1080 * 3];

static void ler(void) {
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
}
static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  int y;
  ler();
  for (y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s/%s.bmp", saida, id);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  fprintf(stderr, "captura: %s\n", nome);
}
// Near-white pixels in a screen rectangle (top-left origin), from the last read.
static int brancos(int x0, int y0, int x1, int y1) {
  int x, y, n = 0;
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++) {
      const unsigned char *p = quadro + ((size_t)(LH - 1 - y) * LW + x) * 3;
      if (p[0] > 235 && p[1] > 235 && p[2] > 235) n++;
    }
  return n;
}

static Uint32 relogio = 100000;
static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    episodios_desenhar();
    stream_folha_desenhar(relogio);
    faixas_desenhar(relogio);
    plrilha_desenhar(relogio);
    SDL_Delay(1);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; faixas_evento(&ev);
  quadros(3);
}


static void faixa(VideoFaixa *f, int numero, const char *rot, const char *idioma) {
  memset(f, 0, sizeof *f);
  f->numero = numero; f->ordinalMkv = -1;
  snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
  snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
}
static void estado(void) {
  VideoSimulacao v;
  memset(&v, 0, sizeof v);
  v.largura = 3840; v.altura = 2160; v.pronto = 1; v.pos = 12.0; v.duracao = 3360.0;
  v.nAudio = 3;
  faixa(&v.audio[0], 1, "Ingl\xc3\xaas  \xc2\xb7  E-AC3 \xc2\xb7 5.1", "en");
  faixa(&v.audio[1], 2, "Portugu\xc3\xaas  \xc2\xb7  AAC \xc2\xb7 2.0", "pt");
  faixa(&v.audio[2], 3, "Espa\xc3\xb1ol  \xc2\xb7  AC3 \xc2\xb7 5.1", "es");
  v.nLeg = 0; v.legAtual = -1;
  video_simular(&v);
  player_shot_estado(relogio, 12.0f, 3360.0f, 1, 0, 0, 0);
  player_shot_esconder();
}
// Red pixels (the island red, 255/90/82) in a rectangle.
static int vermelhos(int x0, int y0, int x1, int y1) {
  int x, y, n = 0;
  ler();
  for (y = y0; y < y1; y++)
    for (x = x0; x < x1; x++) {
      const unsigned char *p = quadro + ((size_t)(LH - 1 - y) * LW + x) * 3;
      if (p[0] > 220 && p[1] > 60 && p[1] < 120 && p[2] > 50 && p[2] < 115) n++;
    }
  return n;
}

int main(int argc, char **argv) {
  int i, r100, r150;
  saida = argc > 1 ? argv[1] : nvt("/nv-cacheboost-shot");
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: volume", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\nfonteInterface 3\n");   // Montserrat: the owner's TV
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  { struct tm lt; time_t t = time(NULL);
    localtime_r(&t, &lt); lt.tm_hour = 20; lt.tm_min = 19; lt.tm_sec = 0;
    plrilha_shot_hora(mktime(&lt)); }
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
    snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
    cat_definir(&c, 1); }
  cacheboost_simular_suporte(1);
  player_abrir(0, NULL);
  player_erro_fonte(); player_limpar_erro_fonte();
  player_shot_video(1);
  estado(); quadros(20);

  // 1. Sheet opened: focus on the track, volume row at 100%.
  faixas_abrir_em(0); estado(); quadros(40);
  r100 = vermelhos(1100, 60, 1900, 600);
  salvar("audio-volume-100");
  // 2. Up to the volume row (past the #202 speed row), five steps right:
  // 150%, red value and bar.
  tecla(SDLK_UP); tecla(SDLK_UP);
  for (i = 0; i < 5; i++) tecla(SDLK_RIGHT);
  assert(cacheboost_volume() == 150);
  estado(); quadros(30);
  r150 = vermelhos(1100, 60, 1900, 600);
  salvar("audio-volume-150");
  fprintf(stderr, "red pixels: 100%% %d, 150%% %d\n", r100, r150);
  assert(r150 > r100 + 200);
  // 3. Max 200%.
  for (i = 0; i < 8; i++) tecla(SDLK_RIGHT);
  assert(cacheboost_volume() == 200);
  estado(); quadros(20); salvar("audio-volume-200");
  // 4. Audio goes out as passthrough: capped at 100, reason shown.
  cacheboost_ganho_relato(CB_GANHO_PASSTHROUGH);
  tecla(SDLK_RIGHT);
  assert(cacheboost_volume() == 100);
  estado(); quadros(20); salvar("audio-volume-passthrough");
  // 5. LG/Samsung: row dimmed, not focusable.
  cacheboost_ganho_relato(CB_GANHO_PCM);
  cacheboost_simular_suporte(0);
  tecla(SDLK_BACKSPACE); quadros(30);
  faixas_abrir_em(0); estado(); quadros(40);
  tecla(SDLK_UP);
  salvar("audio-volume-indisponivel");
  // 6. #202: the speed row below the volume; two steps right = 1,5x, live,
  // the value in the accent; the island's header pill carries "1,5x" too.
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  assert(video_velocidade_atual() == 150);
  estado(); quadros(30); salvar("audio-velocidade-150");
  // 7. Audio goes out as passthrough while at 1,5x: back to 1x, the row is
  // dimmed with the reason and the focus cannot land on it.
  cacheboost_ganho_relato(CB_GANHO_PASSTHROUGH);
  estado(); quadros(30);
  assert(video_velocidade_atual() == 100);
  salvar("audio-velocidade-receptor");
  cacheboost_ganho_relato(CB_GANHO_PCM);
  puts("cacheboost_shot: 7 captures ok");
  return 0;
}
