// Live statistics fit: the panel with the C9 photo's data (long add-on, long
// host, HEVC Dolby Vision, E-AC-3 5.1 Atmos) at one output size, language and
// Interface size. Every text box stays inside its card, short values are never
// cut, the panel stays on screen and the hint row shows whenever it has focus.
//   estatvivo_cabe <width> <height> <language> <png prefix>   (NUVIO_TAMANHO_UI sets the scale)
#include "ajustes.h"
#include "ajustes_ux.h"
#include "estatvivoui.h"
#include "gfx.h"
#include "perfiltv.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int W, H, falhas;
static GLuint fbo;
#define CONFERE(c, ...) do { if (!(c)) { printf("FAIL %s: ", estado); printf(__VA_ARGS__); printf("\n"); falhas++; } } while (0)

static int dentro(GfxRect a, GfxRect b) {
  return a.x >= b.x - 1 && a.y >= b.y - 1 && a.x + a.w <= b.x + b.w + 1 && a.y + a.h <= b.y + b.h + 1;
}

static void quadros(int n) {
  for (int i = 0; i < n; i++) {
    txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, W, H);
    glClearColor(0.07f, 0.07f, 0.08f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_cor((GfxRect){ 0, 160, 1920, 760 }, 0, 0.42f, 0.36f, 0.28f, 1);   // letterboxed film
    estatvivo_desenhar(SDL_GetTicks());
  }
}

static void png(const char *prefixo, const char *estado) {
  char nome[600];
  unsigned char *pix = malloc((size_t)W * H * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, W, H, 32, SDL_PIXELFORMAT_RGBA32);
  assert(pix && s);
  glFinish();
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (int y = 0; y < H; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (size_t)(H - 1 - y) * W * 4, (size_t)W * 4);
  snprintf(nome, sizeof nome, "%s-%s.png", prefixo, estado);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
}

static void conferir(const char *estado, int dicas) {
  EvGeometriaTeste g;
  float p[4], d[4];
  estatvivo_teste_layout(p, d);
  estatvivo_teste_geometria(&g);
  GfxRect painel = { p[0], p[1], p[2], p[3] };
  GfxRect tela = { 0, 0, 1920.0f / g.escala, 1080.0f / g.escala };
  CONFERE(dentro(painel, tela), "panel %.0f,%.0f %.0fx%.0f outside screen %.0fx%.0f", p[0], p[1], p[2], p[3], tela.w, tela.h);
  CONFERE(!dicas || g.nKeys >= 3, "hint row missing (%d keys)", g.nKeys);
  CONFERE(!dicas || (strstr(g.dicas, "↓") && strstr(g.dicas, "|")), "pin/close hints missing");
  CONFERE(!dicas || g.hintTextH >= g.bodyTextH, "hint text height %.1f smaller than body %.1f", g.hintTextH, g.bodyTextH);
  CONFERE(!dicas || g.hintTextH <= g.keys[0].h, "hint text exceeds row height");
  CONFERE(estatvivo_tamanho() == EV_TAM_MINIMO || g.nTextos > 10, "rendered text bounds missing");
  for (int i = 0; i < g.nKeys; i++)
    CONFERE(dentro(g.keys[i], tela) && dentro(g.keys[i], painel), "hint %d at %.0f,%.0f off screen/panel", i, g.keys[i].x, g.keys[i].y);
  float titleH = 0;
  for (int i = 0; i < g.nTiles; i++) {
    int found = 0;
    for (int j = 0; j < g.nTextos; j++) {
      if (g.donos[j].x != g.tiles[i].x || g.donos[j].y != g.tiles[i].y) continue;
      // The header is drawn first in each card; wrapped lines keep this height.
      float h = g.textos[j].h;
      if (!titleH) titleH = h;
      CONFERE(h == titleH, "card %d title height %.1f differs from %.1f", i, h, titleH);
      found = 1;
      break;
    }
    CONFERE(found, "card %d title missing", i);
    CONFERE(dentro(g.tiles[i], painel), "card %d %.0f,%.0f %.0fx%.0f outside panel", i, g.tiles[i].x, g.tiles[i].y, g.tiles[i].w, g.tiles[i].h);
    CONFERE(!dicas || g.tiles[i].y + g.tiles[i].h < g.separator.y, "card %d overlaps hint row", i);
  }
  for (int i = 0; i < g.nTextos; i++) {
    GfxRect t = g.textos[i], o = g.donos[i];
    CONFERE(dentro(t, o), "text %.0f,%.0f %.0fx%.0f outside card %.0f,%.0f %.0fx%.0f",
            t.x, t.y, t.w, t.h, o.x, o.y, o.w, o.h);
  }
  CONFERE(g.cortes == 0, "%d short lines cut with an ellipsis", g.cortes);
}

static int tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return estatvivo_evento(&e, 0);
}

int main(int argc, char **argv) {
  static const char *const L[] = { "pt", "en", "ro", "uk", "ru", "fr", "de" };
  const char *dir = getenv("NUVIO_DADOS"), *prefixo;
  char arq[700];
  int idioma = 1;
  FILE *f;
  assert(argc == 5 && dir && *dir);
  W = atoi(argv[1]); H = atoi(argv[2]); prefixo = argv[4];
  for (int i = 0; i < 7; i++) if (!strcmp(argv[3], L[i])) idioma = i;
  snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
  f = fopen(arq, "w"); assert(f);
  // The C9 in the photo uses Montserrat, the widest proportional family.
  fprintf(f, "idioma %d\nanimacoes 0\nfonteInterface %s\n", idioma, getenv("NUVIO_TESTE_FONTE") ? getenv("NUVIO_TESTE_FONTE") : "3");
  fclose(f);
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("estatvivo_cabe", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w && SDL_GL_CreateContext(w));
  GLuint tex;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  gfx_tamanho_alvo(W, H);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", (float)W / 1920.0f));
  ajustes_recursos("deploy/app/art");
  gfx_icones_dir("deploy/app/art");
  tex_iniciar(64);

  // The C9 photo: LG, estimated speed, buffer in seconds, server from the link.
  EvTelaTeste t = { 0 };
  snprintf(t.titulo, sizeof t.titulo, "Matchbox the Movie");
  snprintf(t.sub, sizeof t.sub, "2026 · 127 min · 4K Dolby Vision");
  snprintf(t.addon, sizeof t.addon, "AIOStreams | ElfHosted Torrentio");
  snprintf(t.resolucao, sizeof t.resolucao, "3840 × 1608");
  snprintf(t.codecHdr, sizeof t.codecHdr, "HEVC · Dolby Vision");
  snprintf(t.taxa, sizeof t.taxa, "26.3 Mbps");
  snprintf(t.audio, sizeof t.audio, "E-AC-3 5.1 Atmos · English");
  t.plataforma = PTV_LG;
  EvEstado e;
  ev_zerar(&e);
  for (int i = 0; i < 25; i++) e.hist[i] = 22000 + (i * 7919) % 9000;
  e.nHist = 25;
  e.velOrigem = EVO_ESTIMADA; e.kbps = 26000;
  e.bufOrigem = EVO_MEDIDO; e.bufSeg = 6; e.bufMb = 19;
  e.servOrigem = EVO_LINK;
  snprintf(e.host, sizeof e.host, "addon.debridstream.elfhosted.com");
  e.quadrosOrigem = EVO_SEM;
  estatvivo_teste(&e, &t);
  estatvivo_abrir();
  SDL_Delay(650);

  const char *estado = "full";
  quadros(120); conferir(estado, 1); png(prefixo, estado);
  // DOWN pins; UP focuses the pinned panel: the hints must come back.
  tecla(SDLK_DOWN); quadros(60);
  tecla(SDLK_UP); quadros(120);
  estado = "pinned-focused";
  assert(estatvivo_aberto() && estatvivo_fixado());
  conferir(estado, 1); png(prefixo, estado);
  tecla(SDLK_UP); quadros(120);
  estado = "unpinned";
  assert(estatvivo_aberto() && !estatvivo_fixado());
  conferir(estado, 1); png(prefixo, estado);
  tecla(SDLK_RETURN); quadros(120);
  estado = "compact";
  assert(estatvivo_tamanho() == EV_TAM_COMPACTO);
  conferir(estado, 1); png(prefixo, estado);
  tecla(SDLK_RETURN); quadros(120);
  estado = "minimal";
  assert(estatvivo_tamanho() == EV_TAM_MINIMO);
  conferir(estado, 1); png(prefixo, estado);
  printf("%s %dx%d %s font %d scale %s: %s\n", prefixo, W, H, argv[3], (int)txt_fonte_interface(),
         getenv("NUVIO_TAMANHO_UI") ? getenv("NUVIO_TAMANHO_UI") : "1",
         falhas ? "FAIL" : "ok");
  return falhas ? 1 : 0;
}
