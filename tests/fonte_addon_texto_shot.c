// Shot of the sources sheet in "Texto das fontes = Do addon" mode, fed by real
// Stremio JSON through stream_extrair (multi-line description + emojis).
//   bash tests/fonte_addon_texto_shot.sh <dir>
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
// Include the sheet to verify its actual row geometry.
#include "../src/streams.c"
#include "plrilha.h"
#include "video.h"
#include "badges.h"
#include "idioma.h"
#include "ilha.h"
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
    else ilha_desenhar(relogio);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  stream_folha_evento(&e);
  quadros(2);
}


#ifdef NV_ADDON_LAYOUT_TEST
static int lastRight, boxCount, checking, lineCount;
static float lastBottom;
void fonte_addon_line(const char *text, float y, float w, float h, float limit) {
  if (!checking) return;
  assert(y >= lastBottom && w <= limit && h > 0);
  lastBottom = y+h; lineCount++;
  (void)text;
}
void fonte_addon_box(int x, int y, int w, int h, int emoji) {
  if (!checking) return;
  assert(x >= lastRight && y >= 0 && w >= 0 && h > 0);
  lastRight = x+w; boxCount++;
  (void)emoji;
}
void fonte_addon_check(const Stream *s) {
  checking=1;
  int lines = 0; lastBottom=0; lineCount=0;
  float height = txt_addon_bloco(TXT_PG_FIM,s->descricao,0,0,10000,255,0,&lines);
  assert(lines == 7 && lineCount == 7 && height >= lastBottom && height >= 7*28);
  const char *emoji[] = {"🇺🇸","🇸🇦","🇯🇵","⚡","🎞️","🔊","📦","⏱️","🏷️","💾"};
  for (unsigned i=0;i<sizeof emoji/sizeof *emoji;i++) {
    int bytes=0,w=0,h=0;
    assert(txt_addon_emoji(emoji[i],&bytes) && bytes == (int)strlen(emoji[i]));
    lastRight=boxCount=0;
    assert(txt_addon_test_surface(TXT_PG_FIM,emoji[i],&w,&h)>0);
    assert(w>0 && h>0 && boxCount==1 && lastRight==w);
  }
  char line[2048]; const char *p=s->descricao;
  while (*p) {
    const char *end=strchr(p,'\n'); size_t n=end?(size_t)(end-p):strlen(p);
    memcpy(line,p,n); line[n]=0;
    int w=0,h=0; lastRight=boxCount=0;
    txt_addon_test_surface(TXT_PG_FIM,line,&w,&h);
    assert(boxCount>0 && lastRight==w && h<=height/7+2);
    if(!end) break;
    p=end+1;
  }
  lastBottom=0;
  float full=txt_addon_bloco(TXT_PG_FIM,"A\n\nB",0,0,500,255,0,&lines);
  assert(lines==3 && full>=84);
  lastBottom=0;
  txt_addon_bloco(TXT_PG_FIM,"📦Averylongfilenamewithoutspaces.mkv🇯🇵",0,0,80,255,0,&lines);
  assert(lines>1);
  checking=0;
  printf("PASS: seven lines; color emoji/flags; non-overlapping run boxes; wrap and blank lines\n");
}
#endif
int main(int argc, char **argv) {
  Stream *v = NULL;
  int count;
  saida = argc > 1 ? argv[1] : nvt("/nv-fonte-addon");
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
  char fixture[8192];
  FILE *input = fopen("tests/fixtures/fonte-addon.json", "rb"); assert(input);
  size_t bytes = fread(fixture, 1, sizeof fixture - 1, input); fclose(input);
  fixture[bytes] = 0;
  count = stream_extrair(fixture, "AIOStreams", &v);
  assert(count == 2);
  stream_definir_alvo("tt14688458:2:5");
  stream_definir_lista(v, count);
  stream_folha_nome("Silo");
  stream_folha_contexto("T2:E5 · Silo");
  ilha_abrir_sessao(); ilha_coberta(0); ilha_relogio_visivel(1);
  stream_folha_abrir();
  gfx_cor((GfxRect){ 0, 0, 1, 1 }, 0, 0, 0, 0, 0);
  quadros(50);
  salvar("1-foco-primeira");
  tecla(SDLK_DOWN);
  quadros(40);
  salvar("2-foco-segunda");
  if (!getenv("NV_CAPTURE_ONLY")) {
    // Seven description lines plus two name lines must survive parsing/layout.
    assert(strchr(v[0].rotulo, '\n'));
    assert(strstr(v[0].descricao, "SUB (AR🇸🇦)"));
    float h = alturaLinha(0, -1, 0);
    assert(h >= 20 + 40 + 7 * 28 + 18);
    assert(h == alturaLinha(0, -1, 1));
#ifdef NV_ADDON_LAYOUT_TEST
    extern void fonte_addon_check(const Stream *s);
    fonte_addon_check(&v[0]);
#endif
  }
  free(v);
  return 0;
}
