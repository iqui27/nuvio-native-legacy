// #499: local parsed fixtures, real source list and player switcher, no network.
#include "catalogo.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include <string.h>
static int cacheIcons;
static void cacheIcon(GfxRect r, const char *name, float red, float green, float blue, float a) {
  if (!strcmp(name,"aj_zap") && r.w == 26) cacheIcons++;
  gfx_icone(r,name,red,green,blue,a);
}
#define gfx_icone cacheIcon
#include "../src/streams.c"
#undef gfx_icone
#include "plrilha.h"
#include "video.h"
#include "badges.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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
    cacheIcons = 0;
    stream_folha_desenhar(relogio);
    if (noPlayer) plrilha_desenhar(relogio);
  }
}


static const char *JSON =
  "{\"streams\":["
  "{\"url\":\"https://example.invalid/1.mkv\",\"name\":\"⚡️ AIOStreams 1080p\",\"description\":\"1080p WEB-DL · Cached\\n2.3 GB\"},"
  "{\"url\":\"https://example.invalid/2.mkv\",\"name\":\"Torrentio 1080p\",\"description\":\"1080p WEB-DL · Unknown\\n2.3 GB\"},"
  "{\"url\":\"https://example.invalid/3.mkv\",\"name\":\"[TB download] Torrentio 1080p\",\"description\":\"1080p WEB-DL · Uncached\\n2.3 GB\"}"
  "]}";

int main(int argc, char **argv) {
  Stream *v = NULL;
  int count;
  int addon = argc > 2 ? atoi(argv[2]) : 1;
  noPlayer = argc > 3 ? atoi(argv[3]) : 0;
  saida = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/499cache/fontes";
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
    fprintf(f, "idioma 0\nselected_theme 2\nvidroLocal 0\nfonteInterface %d\nfonteTextoLocal %d\n",
            TXT_FAMILIA_MONTSERRAT, addon);
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  count = stream_extrair(JSON, "AIOStreams | ElfHosted", &v);
  assert(count == 3);
  assert(v[0].emCache && !v[1].emCache && v[2].foraCache);
  char title[96]; tituloAddon(&v[0],title,sizeof title);
  assert(!strcmp(title,"AIOStreams 1080p"));
  assert(!strcmp(v[0].rotulo,"⚡️ AIOStreams 1080p"));
  if (noPlayer) {
    CatItem c = {0}; VideoSimulacao vs = {0};
    snprintf(c.tipo,sizeof c.tipo,"series");
    snprintf(c.titulo,sizeof c.titulo,"Silo");
    cat_definir(&c,1); player_abrir(0,NULL); player_shot_video(1);
    vs.largura=1920; vs.altura=1080; vs.pronto=1; vs.pos=12; vs.duracao=3360;
    video_simular(&vs); player_shot_estado(relogio,12,3360,1,0,0,0);
    player_shot_esconder(); quadros(10);
  }
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, count);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");
  stream_folha_abrir();
  gfx_cor((GfxRect){ 0, 0, 1, 1 }, 0, 0, 0, 0, 0);
  quadros(50);
  assert(cacheIcons == 1); // row icon only; the cache filter has its own zap
  salvar("lista");
  free(v);
  puts("PASS: cached icon only, original name preserved, cached/unknown/uncached visible.");
  return 0;
}
