// CAPTURA DAS MARCAS DE FORMATO no player (selos do alto a direita) e na tabela
// de amostras. Nao entra na suite: precisa de janela GL e de olho humano.
//
// O player le o formato de video_largura/video_hdr/video_tem_*, que no Mac sao
// stubs fixos em zero (e video_pronto em 0, que deixa o player em "abrindo"). logos_formato_shot.sh compila video.c com esses quatro
// nomes trocados e este arquivo os define, para o selo aparecer:
//
//   bash tests/logos_formato_shot.sh /tmp/nv-logos-shots/depois/player
#include "player.h"
#include "nvtmp.h"
#include "catalogo.h"
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cLarg = 3840, cDV = 1, cAtmos = 1;
static const char *cHdr = "DolbyVision";
int  video_pronto(void) { return 1; }
int  video_ativo(void) { return 1; }
int  video_tocar(const char *u) { (void)u; return 1; }
int  video_tocando(void) { return 1; }
int  video_largura(void) { return cLarg; }
int  video_tem_dolby_vision(void) { return cDV; }
int  video_tem_atmos(void) { return cAtmos; }
const char *video_hdr(void) { return cHdr; }

static void salva(const char *nome, SDL_Window *win) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  (void)win;
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s); free(pix);
  printf("captura: %s\n", nome);
}
static void quadros(SDL_Window *win, const char *nome, int amostras) {
  for (int i = 0; i < 90; i++) {
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, SDL_GetTicks());
    glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    if (amostras) {
      // Amostras por contexto, sobre fundo escuro e sobre claro (foco).
      static const FormatoMarca F[] = { FMT_4K, FMT_1080, FMT_720, FMT_SDR, FMT_HDR, FMT_HDR10,
        FMT_HDR10P, FMT_HLG, FMT_DV, FMT_ATMOS, FMT_DTS, FMT_DTSX, FMT_DTSHD, FMT_TRUEHD,
        FMT_DD, FMT_DDP, FMT_IMAX, FMT_IMAX_ENH, FMT_AV1, FMT_HEVC, FMT_AVC, FMT_REMUX };
      for (int fundo = 0; fundo < 2; fundo++) {
        float y0 = 60 + fundo * 500, k = fundo ? .1f : .94f;
        gfx_cor((GfxRect){ 0, y0 - 20, 1920, 470 }, 0, fundo ? .93f : .08f, fundo ? .93f : .08f, fundo ? .94f : .1f, 1);
        for (int h = 0; h < 3; h++) {
          float alt = h == 0 ? 26.f : h == 1 ? 34.f : 44.f, x = 40;
          for (size_t j = 0; j < sizeof F / sizeof *F; j++) {
            x += marca_formato(F[j], x, y0 + h * 130, alt, k, k, k, 1) + 22;
            if (x > 1800) break;
          }
        }
      }
    } else player_desenhar(SDL_GetTicks());
    if (i == 89) salva(nome, win);
    SDL_GL_SwapWindow(win); SDL_Delay(12);
  }
}
int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-logos-player");
  char nome[600];
  SDL_Window *w; SDL_GLContext gl;
  CatItem c = {0};
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: marcas", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  snprintf(c.tipo, sizeof c.tipo, "movie"); snprintf(c.titulo, sizeof c.titulo, "Filme de teste");
  snprintf(c.imdb, sizeof c.imdb, "tt0000001");
  cat_definir(&c, 1); player_abrir(0, NULL);
  player_definir_fonte("http://exemplo.invalido/filme.mkv");
  { SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; player_evento(&e); }
  snprintf(nome, sizeof nome, "%s-4k-dv-atmos.bmp", saida); quadros(w, nome, 0);
  cLarg = 3840; cDV = 0; cAtmos = 0; cHdr = "HDR10";
  snprintf(nome, sizeof nome, "%s-4k-hdr10.bmp", saida); quadros(w, nome, 0);
  cLarg = 1920; cDV = 0; cAtmos = 1; cHdr = "none";
  snprintf(nome, sizeof nome, "%s-hd-atmos.bmp", saida); quadros(w, nome, 0);
  snprintf(nome, sizeof nome, "%s-amostras.bmp", saida); quadros(w, nome, 1);
  return 0;
}
