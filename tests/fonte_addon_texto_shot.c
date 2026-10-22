// Shot of the sources sheet in "Texto das fontes = Do addon" mode, fed by real
// Stremio JSON through stream_extrair (multi-line description + emojis).
//   bash tests/fonte_addon_texto_shot.sh <dir>
#include "catalogo.h"
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
#include "badges.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;
enum { LW = 1920, LH = 1080 };
static const char *saida;
static unsigned char quadro[LW * LH * 3];
static int noPlayer;
static Uint32 relogio = 100000;

static void salvar(const char *id) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  char nome[700];
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, quadro);
  for (int y = 0; y < LH; y++)
    memcpy((unsigned char *)s->pixels + y * s->pitch, quadro + (size_t)(LH - 1 - y) * LW * 3, (size_t)LW * 3);
  snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8);
    if (noPlayer) player_atualizar(1.f / 60, relogio);
    stream_folha_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(0.025f, 0.025f, 0.03f, 1); glClear(GL_COLOR_BUFFER_BIT);
    gfx_novo_quadro();
    if (noPlayer) player_desenhar(relogio);
    else {
      gfx_cor((GfxRect){ 0, 0, LW, LH }, 0, .26f, .17f, .12f, 1);
      gfx_cor((GfxRect){ 0, 0, LW, 360 }, 0, .55f, .36f, .22f, 1);
    }
    stream_folha_desenhar(relogio);
    if (noPlayer) plrilha_desenhar(relogio);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  stream_folha_evento(&e);
  quadros(2);
}


// Real add-on JSON, parsed by the app's own parser, shown with
// Ajustes > Texto das fontes = "Do addon" (fonteTextoLocal 1).
static const char *JSON =
  "{\"streams\":["
  "{\"url\":\"https://x.invalid/1.mkv\",\"name\":\"[RD+] AIOStreams 4K\","
  "\"description\":\"\xF0\x9F\x92\xBE 12.3 GB \xF0\x9F\x91\xA4 45\\n\xF0\x9F\x8E\x9E\xEF\xB8\x8F HEVC \xE2\x80\xA2 HDR10 \xE2\x80\xA2 DV\\n"
  "\xF0\x9F\x94\x8A DDP 5.1 Atmos\\n\xF0\x9F\x8C\x90 EN | PT\\n\xE2\x9A\x99\xEF\xB8\x8F RD+ \xE2\x9A\xA1 cached \xF0\x9F\x8F\xB7\xEF\xB8\x8F WEB-DL \xF0\x9F\x93\xA6 Silo.S02E05.2160p.WEB-DL.DV.HDR10.HEVC.DDP5.1.Atmos-GROUP.mkv\"},"
  "{\"url\":\"https://x.invalid/2.mkv\",\"name\":\"Torrentio\\n1080p\","
  "\"title\":\"Silo.S02E05.1080p.WEB-DL.x265.mkv\\n\xF0\x9F\x91\xA4 120 \xF0\x9F\x92\xBE 2.3 GB \xE2\x9A\x99\xEF\xB8\x8F YTS\\n\xF0\x9F\x87\xA7\xF0\x9F\x87\xB7 Dublado\"},"
  "{\"url\":\"https://x.invalid/3.mkv\",\"name\":\"Comet 720p\",\"description\":\"\xE2\x8F\xB1\xEF\xB8\x8F 1h 40m\\n720p WEB\"},"
  "{\"url\":\"https://x.invalid/4.mkv\",\"name\":\"One line\",\"description\":\"900 MB\"}"
  "]}";

int main(int argc, char **argv) {
  Stream *v = NULL;
  int count;
  saida = argc > 1 ? argv[1] : "/tmp/nv-fonte-addon";
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: texto do addon", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\nselected_theme 2\nvidroLocal 0\nfonteInterface %d\nfonteTextoLocal 1\n",
            TXT_FAMILIA_MONTSERRAT);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  count = stream_extrair(JSON, "AIOStreams | ElfHosted", &v);
  assert(count == 4);
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, count);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");
  stream_folha_abrir();
  gfx_cor((GfxRect){ 0, 0, 1, 1 }, 0, 0, 0, 0, 0);
  quadros(50);
  salvar("1-foco-primeira");
  tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  quadros(40);
  salvar("2-foco-terceira");
  return 0;
}
