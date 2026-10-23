// CAPTURA DOS SELOS EMBUTIDOS (padrao e colorido do Xperience) na folha de
// fontes, com nomes de release variados: remux de grupo, WEB-DL com DV+HDR10+,
// anime com 10bit, HDTV, DVDRip, edicoes. NUVIO_SHOT_SELOS=1 liga o colorido.
// Fora da suite (*_shot.sh): janela GL e olho humano.
#include "catalogo.h"
#include "nvtmp.h"
#include "streams.h"
#include "fontepref.h"
#include "badges.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "selospacote.h"
#include "vidro_fundo.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fonte(Stream *s, const char *arquivo, int altura, int mp4, int dv) {
  memset(s, 0, sizeof *s);
  snprintf(s->provedor, sizeof s->provedor, "AIOStreams | ElfHosted");
  snprintf(s->rotulo, sizeof s->rotulo, "\xe2\x9a\xa1\xef\xb8\x8e  Exemplo");
  snprintf(s->descricao, sizeof s->descricao, "8.2 GB  |   22.1 Mbps  |\n%s", arquivo);
  snprintf(s->arquivo, sizeof s->arquivo, "%s", arquivo);
  snprintf(s->url, sizeof s->url, "https://exemplo.invalido/%s", arquivo);
  s->altura = altura; s->mp4 = mp4; s->dolbyVision = dv;
  s->tamanhoMB = altura >= 2160 ? 11264 : 2048;
  s->badges = badges_detectar(arquivo);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 60; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    stream_folha_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    if (vidroFundoAtivo()) vidroFundoDesenhar();
    else {
      gfx_cor((GfxRect){ 0, 0, 1920, 1080 }, 0, .26f, .17f, .12f, 1);
      gfx_cor((GfxRect){ 0, 0, 1920, 360 }, 0, .55f, .36f, .22f, 1);
      gfx_cor((GfxRect){ 900, 420, 900, 260 }, 0, .82f, .78f, .70f, 1);
    }
    stream_folha_desenhar(SDL_GetTicks());
    if (i == 59) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  stream_folha_evento(&e);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-selos");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  Stream v[6];
  int i;

  { const char *dir = getenv("NUVIO_DADOS");
    if (dir && *dir) {
      char caminho[700];
      FILE *f;
      snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
      f = fopen(caminho, "w"); assert(f);
      fprintf(f, "idioma 0\nselected_theme 2\nvidroLocal 1\n");
      if (getenv("NUVIO_SHOT_SELOS")) fprintf(f, "selosColoridosLocal 0\n");
      fclose(f);
      ajustes_dir(dir);
    } }

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: selos", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  vidroFundoPreparar();
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  selospacote_dir_embutidos("deploy/app/art");   // quem faz isto no app e home.c

  fonte(&v[0], "Dune.Part.Two.2024.2160p.WEB-DL.DV.HDR10+.DDP5.1.Atmos.H.265-FLUX.mkv", 2160, 0, 1);
  fonte(&v[1], "The.Matrix.1999.2160p.UHD.BluRay.REMUX.HDR.HEVC.TrueHD.7.1.Atmos-FraMeSToR.mkv", 2160, 0, 0);
  fonte(&v[2], "[Judas] Anime S01E01 [1080p][HEVC x265 10bit][Multi-Subs] Crunchyroll.mkv", 1080, 0, 0);
  fonte(&v[3], "Old.Movie.1970.1080p.BluRay.Remastered.Extended.Cut.FLAC.2.0.x264-CtrlHD.mkv", 1080, 0, 0);
  fonte(&v[4], "Film.2022.1080p.AMZN.WEB-DL.DDP5.1.H.264.IMAX-NTb.mkv", 1080, 0, 0);
  fonte(&v[5], "Show.S02E03.720p.HDTV.x264-LOL.mp4", 720, 1, 0);
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, 6);
  stream_folha_nome("Exemplo");
  stream_folha_contexto("T2:E5 · Exemplo");
  stream_definir_atual(0);
  stream_folha_abrir();
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha.bmp", saida);
  captura(nome, w);
  for (i = 0; i < 2; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-folha-foco.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas dos selos embutidos gravadas.");
  return 0;
}
