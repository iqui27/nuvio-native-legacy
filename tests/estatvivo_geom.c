// Live statistics: clock alignment, internal footer, animated side changes and
// pinned focus navigation. Render the real panel into an offscreen GL target.
#include "ajustes.h"
#include "ajustes_ux.h"
#include "dados.h"
#include "estatvivoui.h"
#include "layout.h"
#include "perfis.h"
#include "plrilha.h"
#include "text.h"
#include "tex_cache.h"
#include "perfiltv.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
#define CONFERE(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)

// Sidebar width, with the same top and screen inset as the player clock.
#define LAT_W 720.0f
#define LAT_TOPO NV_ILHA_Y

static int key(SDL_Keycode k) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return estatvivo_evento(&e, 0);
}

static void frame(int n) {
  for (int i = 0; i < n; i++) {
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    glClear(GL_COLOR_BUFFER_BIT);
    estatvivo_desenhar(SDL_GetTicks());
  }
}

static void shot(const char *path) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  unsigned char *pixels = malloc(1920 * 1080 * 4);
  assert(s && pixels);
  glFinish();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, path) == 0);
  free(pixels);
  SDL_FreeSurface(s);
}

#define GEO_PAD 32.0f
#define GEO_GAP 12.0f
#define GEO_KEY_H 32.0f

static int corpoChamado;
static void corpoFake(GfxRect c, float a, void *u) { (void)c; (void)a; (void)u; corpoChamado++; }
static void shotGeom(const char *name) {
  char path[512];
  snprintf(path, sizeof path, "%s/%s.png",
           getenv("NUVIO_REV_PREFIX") ? getenv("NUVIO_REV_PREFIX") : "/Volumes/ExternalSSD/tmp/rev/estatpainel4",
           name);
  shot(path);
}

static void checkGeometry(int t, const float *panel, const float *hints) {
  EvGeometriaTeste geo;
  int i;
  float left = panel[0] + GEO_PAD, right = panel[0] + panel[2] - GEO_PAD;
  estatvivo_teste_geometria(&geo);
  if (t == EV_TAM_MINIMO) {
    CONFERE(panel[3] > 56.0f, "focused minimal: footer reserved, got %.1f", panel[3]);
    CONFERE(geo.nKeys >= 3 && hints[2] > 0, "focused minimal: hints visible");
  } else {
    CONFERE(hints[2] > 0 && geo.nKeys >= 3, "size %d: hint row visible", t);
    CONFERE(fabsf(hints[0] - left) < 0.5f && fabsf(hints[0] + hints[2] - right) < 0.5f,
            "mode %d: footer follows the content margins", t);
    CONFERE(fabsf(geo.separator.x - left) < 0.5f && fabsf(geo.separator.x + geo.separator.w - right) < 0.5f,
            "mode %d: separator aligned to content", t);
    CONFERE(fabsf(hints[3] - GEO_KEY_H * (t == EV_TAM_COMPLETO ? 1 : 2)) < 0.5f,
            "mode %d: hint rows reserved", t);
    CONFERE(!strstr(geo.dicas, "Menu"), "mode %d: hints never name a Menu key (%s)", t, geo.dicas);
    CONFERE(fabsf(panel[1] + panel[3] - hints[1] - hints[3] - GEO_PAD) < 0.5f,
            "mode %d: footer bottom padding", t);
    for (i = 0; i < geo.nKeys; i++) {
      GfxRect k = geo.keys[i];
      CONFERE(k.y >= hints[1] && k.y + k.h <= hints[1] + hints[3] && k.x >= left && k.x + k.w <= right,
              "mode %d hint %d: inside footer, no overflow", t, i);
      CONFERE(i == 0 || k.y > geo.keys[i - 1].y || k.x > geo.keys[i - 1].x + geo.keys[i - 1].w,
              "mode %d hint %d: no overlap", t, i);
    }
  }
  if (t == EV_TAM_COMPLETO) {
    CONFERE(geo.nTiles == 6, "full mode: six default tiles");
    for (i = 0; i < geo.nTiles; i++) {
      GfxRect r = geo.tiles[i];
      CONFERE(fabsf(r.x - (i % 2 ? left + (right - left - GEO_GAP) * 0.5f + GEO_GAP : left)) < 0.5f,
              "full tile %d: left margin", i);
      CONFERE(r.y >= panel[1] + GEO_PAD && r.x + r.w <= right + 0.5f,
              "full tile %d: inside padded content", i);
      if (i >= 2) CONFERE(fabsf(r.y - geo.tiles[i - 2].y - geo.tiles[i - 2].h - GEO_GAP) < 0.5f,
                          "full tile %d: vertical tile gap", i);
    }
    CONFERE(fabsf(geo.tiles[1].x - geo.tiles[0].x - geo.tiles[0].w - GEO_GAP) < 0.5f,
            "full mode: horizontal tile gap");
    CONFERE(fabsf(geo.header.x - geo.tiles[0].x) < 0.5f &&
            fabsf(geo.header.x + geo.header.w - geo.tiles[1].x - geo.tiles[1].w) < 0.5f,
            "header aligns exactly with tile grid");
    CONFERE(geo.meta.x >= geo.header.x && geo.meta.y >= geo.header.y &&
            geo.meta.x + geo.meta.w <= geo.header.x + geo.header.w + 0.5f &&
            geo.meta.y + geo.meta.h <= geo.header.y + geo.header.h,
            "badge data moved inside the subtitle content box");
  }
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  int t, i;
  float d[EV_TAM_N][4], e[EV_TAM_N][4], g[4];
  EvEstado est;
  EvTelaTeste tela;
  assert(dir && *dir);
  ajustes_dir(dir);
  dados_iniciar(NULL);

  for (t = 0; t < EV_TAM_N; t++) {
    estatvivo_retangulo(t, 0, d[t]);
    estatvivo_retangulo(t, 1, e[t]);
  }
  CONFERE(fabsf(d[EV_TAM_COMPLETO][2] - LAT_W) < 0.5f, "Completo na largura da lateral: %.1f", d[EV_TAM_COMPLETO][2]);
  CONFERE(fabsf(d[EV_TAM_COMPLETO][0] - (NV_TELA_W - NV_ILHA_MARGEM_D - LAT_W)) < 0.5f, "Completo no x da lateral: %.1f", d[EV_TAM_COMPLETO][0]);
  for (t = 0; t < EV_TAM_N; t++) {
    CONFERE(fabsf(d[t][0] + d[t][2] - (NV_TELA_W - NV_ILHA_MARGEM_D)) < 0.5f, "tam %d: direita na margem", t);
    CONFERE(fabsf(e[t][0] - NV_ILHA_MARGEM_D) < 0.5f, "tam %d: esquerda na margem", t);
    CONFERE(fabsf(d[t][1] - LAT_TOPO) < 0.5f && fabsf(e[t][1] - LAT_TOPO) < 0.5f, "size %d: aligned to clock top", t);
    CONFERE(d[t][2] == e[t][2] && d[t][3] == e[t][3], "tam %d: o lado esquerdo e o espelho do direito", t);
    CONFERE(e[t][0] + e[t][2] / 2 + (d[t][0] + d[t][2] / 2) > 0 && fabsf((e[t][0] - NV_ILHA_MARGEM_D) - (NV_TELA_W - NV_ILHA_MARGEM_D - (d[t][0] + d[t][2]))) < 0.5f,
            "tam %d: x espelhado", t);
    CONFERE(d[t][1] + d[t][3] <= NV_TELA_H - 36.0f, "tam %d: cabe na altura da tela (%.0f)", t, d[t][1] + d[t][3]);
  }
  CONFERE(d[EV_TAM_COMPACTO][2] < d[EV_TAM_COMPLETO][2] && d[EV_TAM_COMPACTO][3] < d[EV_TAM_COMPLETO][3], "Compacto menor que o Completo");
  CONFERE(d[EV_TAM_MINIMO][2] < d[EV_TAM_COMPLETO][2] && d[EV_TAM_MINIMO][3] < d[EV_TAM_COMPACTO][3], "Minimo menor que o Compacto");

  // O mesmo retangulo, quadro a quadro, enquanto os numeros mudam (tamanho e lado
  // do painel aberto, nao dos valores).
  for (t = 0; t < EV_TAM_N; t++)
    for (i = 0; i < 40; i++) {
      memset(&est, 0, sizeof est);
      memset(&tela, 0, sizeof tela);
      est.kbps = i * 977 % 90000; est.bufSeg = (double)(i * 13 % 100); est.perdidos = i * 31; est.quadros = 100000 + i;
      snprintf(tela.titulo, sizeof tela.titulo, "%s", i & 1 ? "Titulo curto" : "Um titulo bem mais comprido para forcar a largura do cartao");
      estatvivo_teste(&est, &tela);
      ajustes_definir_estat_lado(i & 1);
      estatvivo_retangulo(t, i & 1, g);
      CONFERE(memcmp(g, (i & 1) ? e[t] : d[t], sizeof g) == 0, "tam %d quadro %d: retangulo muda com os valores", t, i);
    }
  ajustes_definir_estat_lado(0);
  char settings[700];
  snprintf(settings, sizeof settings, "%s/ajustes.txt", dir);
  FILE *f = fopen(settings, "w");
  assert(f);
  fputs("idioma 1\n", f);
  fclose(f);
  ajustes_dir(dir);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Statistics layout", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  assert(context);
  GLuint texture, fbo;
  glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  ajustes_recursos("deploy/app/art");
  char art[1024];
  assert(realpath("deploy/app/art", art));
  gfx_icones_dir(art);
  tex_iniciar(64);
  GfxRect clock;
  for (i = 0; i < 120; i++) {
    plrilha_relogio(1, 3600);
    plrilha_desenhar(SDL_GetTicks());
  }
  assert(plrilha_rect(&clock));
  memset(&tela, 0, sizeof tela);
  snprintf(tela.titulo, sizeof tela.titulo, "Animals");
  snprintf(tela.sub, sizeof tela.sub, "2026 · 106 min · 4K Dolby Vision");
  snprintf(tela.audio, sizeof tela.audio, "E-AC-3 5.1 · English");
  snprintf(tela.resolucao, sizeof tela.resolucao, "3840 × 2160");
  snprintf(tela.codecHdr, sizeof tela.codecHdr, "Dolby Vision");
  tela.plataforma = PTV_ANDROID;
  ev_zerar(&est);
  est.velOrigem = est.bufOrigem = est.servOrigem = est.quadrosOrigem = EVO_MEDIDO;
  est.kbps = 53100; est.mediaKbps = 62000; est.picoKbps = 113000;
  est.bufSeg = 46; est.bufAlvo = 50; est.conexoes = 1;
  est.primeiroByteMs = 448; est.quadros = 589;
  snprintf(est.host, sizeof est.host, "20.download.real-debrid.com");
  estatvivo_teste(&est, &tela);
  estatvivo_abrir();
  SDL_Delay(650);
  float panel[4], hints[4];
  frame(1);
  estatvivo_teste_layout(panel, hints);
  CONFERE(panel[2] < LAT_W - 1 && panel[3] < d[0][3] - 1, "opening grows from clock, not final size");
  CONFERE(fabsf(panel[1] - clock.y) < 0.5f, "opening shares clock top");
  for (t = 0; t < EV_TAM_N; t++) {
    frame(120);
    estatvivo_teste_layout(panel, hints);
    CONFERE(fabsf(panel[1] - clock.y) < 0.5f, "size %d: panel aligned to clock top", t);
    CONFERE(fabsf(panel[0] + panel[2] - clock.x - clock.w) < 0.5f, "size %d: clock side margin", t);
    shotGeom(t == EV_TAM_COMPLETO ? "mode-completo" : t == EV_TAM_COMPACTO ? "mode-compacto" : "mode-minimo");
    checkGeometry(t, panel, hints);
    key(SDLK_RETURN);
  }
  // Clock and panel both lay out in the virtual screen; gfx scales them once.
  for (int s = 0; s < 3; s++) {
    float scale = (float[]){1.0f, 1.2f, 1.5f}[s];
    gfx_escala_ui_definir(scale);
    for (t = 0; t < EV_TAM_N; t++) {
      frame(120);
      estatvivo_teste_layout(panel, hints);
      CONFERE(fabsf(panel[1] - clock.y) < 0.5f, "scale %.1f size %d: virtual clock top", scale, t);
      EvGeometriaTeste scaled;
      estatvivo_teste_geometria(&scaled);
      CONFERE(fabsf(1920.0f / scaled.escala - panel[0] - panel[2] - NV_ILHA_MARGEM_D) < 0.5f,
              "scale %.1f size %d: virtual clock margin", scale, t);
      estatvivo_retangulo(t, 1, g);
      CONFERE(fabsf(g[0] - NV_ILHA_MARGEM_D) < 0.5f && fabsf(g[1] - clock.y) < 0.5f,
              "scale %.1f size %d: left uses clock inset and top", scale, t);
      key(SDLK_RETURN);
    }
  }
  gfx_escala_ui_definir(1);
  frame(120);
  // Same checklist controls as Control Center; MENU also opens the Edit hint.
  key(SDLK_MENU);
  key(SDLK_RETURN); // Remove speed.
  key(SDLK_RIGHT); // Move buffer after source.
  key(SDLK_ESCAPE);
  char *saved = dados_ler("estatvivo-tiles-p1-0.txt");
  CONFERE(saved && !strcmp(saved, "2 1 3 4 5\n"), "full tile selection and order persisted");
  free(saved);
  key(SDLK_RETURN); key(SDLK_RETURN); // Minimal: its own speed/buffer defaults.
  key(SDLK_MENU); key(SDLK_RETURN); key(SDLK_ESCAPE);
  saved = dados_ler("estatvivo-tiles-p1-2.txt");
  CONFERE(saved && !strcmp(saved, "1\n"), "minimal tile selection persisted separately");
  free(saved);
  // Force profile reload; verify real rendering, independent settings and defaults.
  int rendered[6];
  perfis_definir_ativo(2);
  frame(120);
  CONFERE(estatvivo_teste_tiles(rendered) == 2 && rendered[0] == 0 && rendered[1] == 1,
          "new profile keeps minimal defaults");
  key(SDLK_MENU); key(SDLK_RIGHT); key(SDLK_ESCAPE);
  frame(120);
  CONFERE(estatvivo_teste_tiles(rendered) == 2 && rendered[0] == 1 && rendered[1] == 0,
          "minimal reorders and renders tiles");
  perfis_definir_ativo(1);
  frame(120);
  CONFERE(estatvivo_teste_tiles(rendered) == 1 && rendered[0] == 1, "minimal loads saved tiles on profile return");
  shotGeom("custom-minimo");
  key(SDLK_RETURN);
  frame(120);
  CONFERE(estatvivo_teste_tiles(rendered) == 5 && !memcmp(rendered, (int[]){2, 1, 3, 4, 5}, 5 * sizeof(int)),
          "full loads and renders saved order independently");
  shotGeom("custom-completo");

  // Empty tile set: deleting the last tile removes the file, and OK on an
  // offered row adds it back instead of saving a blank line that reverts.
  dados_apagar("estatvivo-tiles-p1-0.txt");
  key(SDLK_MENU); frame(2);
  for (int j = 0; j < 5; j++) key(SDLK_RETURN);   // remove all five
  CONFERE(!dados_ler("estatvivo-tiles-p1-0.txt"), "removing the last tile deletes the file");
  key(SDLK_RETURN);   // re-add the offered tile
  saved = dados_ler("estatvivo-tiles-p1-0.txt");
  CONFERE(saved && !strcmp(saved, "0\n"), "editor repopulates an empty tile set");
  free(saved);
  key(SDLK_RETURN);   // remove it again -> empty
  for (int j = 0; j < 5; j++) key(SDLK_DOWN);
  key(SDLK_RETURN);   // add the last offered tile
  key(SDLK_ESCAPE);
  frame(2);   // a troca de tiles so aparece depois que a edicao sai
  CONFERE(estatvivo_teste_tiles(rendered) == 1 && rendered[0] == 5,
          "empty set accepts a new tile via OK");
  key(SDLK_MENU); frame(120);
  shotGeom("editor");
  key(SDLK_ESCAPE);
  frame(120);   // Settle the last size change before measuring side motion.
  key(SDLK_DOWN);
  CONFERE(estatvivo_fixado() && !estatvivo_aberto(), "DOWN pins and returns focus");
  CONFERE(!key(SDLK_LEFT) && !key(SDLK_RETURN), "pinned player keeps seek/OK");
  {
    SDL_Event up = {0};
    up.type = SDL_KEYDOWN; up.key.keysym.sym = SDLK_UP;
    CONFERE(!estatvivo_evento(&up, 1) && !estatvivo_aberto(), "UP stays with player when it owns UP");
  }
  CONFERE(key(SDLK_UP) && estatvivo_aberto() && estatvivo_fixado(), "UP focuses pinned panel");
  key(SDLK_LEFT);
  CONFERE(ajustes_estat_lado() && estatvivo_fixado(), "LEFT moves pinned panel");
  frame(1);
  estatvivo_teste_layout(panel, hints);
  CONFERE(panel[0] > NV_ILHA_MARGEM_D && panel[0] < d[0][0], "pinned side switch animates");
  frame(120);
  estatvivo_teste_layout(panel, hints);
  CONFERE(fabsf(panel[0] - NV_ILHA_MARGEM_D) < 0.5f, "pinned panel settles left");
  { EvGeometriaTeste pg; estatvivo_teste_geometria(&pg);
    CONFERE(pg.nKeys >= 3 && hints[2] > 0, "focused pinned: hints visible"); }
  shotGeom("pinned-left");
  key(SDLK_RIGHT);
  CONFERE(!ajustes_estat_lado() && estatvivo_fixado(), "RIGHT moves pinned panel");
  frame(120);
  shotGeom("pinned-right");
  key(SDLK_RETURN);
  CONFERE(estatvivo_tamanho() == EV_TAM_COMPACTO && estatvivo_fixado(), "OK resizes while pinned");
  key(SDLK_ESCAPE);
  CONFERE(!estatvivo_aberto() && estatvivo_fixado() && !key(SDLK_RETURN), "Back returns focus, preserves pin");
  key(SDLK_UP); key(SDLK_DOWN);
  CONFERE(!estatvivo_aberto() && estatvivo_fixado(), "DOWN returns focus, preserves pin");
  key(SDLK_UP); key(SDLK_UP);
  CONFERE(estatvivo_aberto() && !estatvivo_fixado(), "UP in panel unpins");
  key(SDLK_ESCAPE); key(SDLK_ESCAPE);
  estatvivo_abrir(); SDL_Delay(650); key(SDLK_DOWN); frame(120); key(SDLK_UP); frame(120);
  CONFERE(estatvivo_aberto() && estatvivo_fixado(), "refocus pinned panel");
  estatvivo_abrir(); frame(120);
  shotGeom("central-unpinned");
  { EvGeometriaTeste ug; estatvivo_teste_geometria(&ug);
    CONFERE(estatvivo_aberto() && !estatvivo_fixado() && ug.nKeys >= 3, "Central on focused pinned panel unpins, hints visible"); }
  estatvivo_fechar(); estatvivo_abrir(); SDL_Delay(650);
  key(SDLK_DOWN); frame(2); key(SDLK_UP); key(SDLK_UP); frame(2);
  key(SDLK_UP); frame(2);   // UP unpinned = Edit (no Menu key needed)
  shotGeom("editor-up");
  key(SDLK_ESCAPE); key(SDLK_ESCAPE);
  CONFERE(!estatvivo_aberto() && !estatvivo_fixado(), "Back closes unpinned panel");

  // P0: player aberto, estatistica FECHADA, lado padrao (direita). O relogio
  // e a folha de Audio pedem a ilha e a estatistica desenha depois (app.c).
  // Fechada ela nao pode esconder a ilha — a pilula e a folha sumiam.
  ajustes_definir_estat_lado(0);
  estatvivo_fechar();
  { PlrIlhaPedido p;
    GfxRect r;
    memset(&p, 0, sizeof p);
    p.w = 720; p.h = 400; p.corpo = corpoFake;
    plrilha_relogio(1, 3600);
    estatvivo_desenhar(SDL_GetTicks());
    plrilha_desenhar(SDL_GetTicks());
    CONFERE(plrilha_rect(&r) && r.w > 8.0f, "relogio vive com estatistica fechada na direita");
    corpoChamado = 0;
    for (i = 0; i < 200 && !corpoChamado; i++) {
      plrilha_pedir(&p);
      estatvivo_desenhar(SDL_GetTicks());
      plrilha_desenhar(SDL_GetTicks());
      SDL_Delay(16);
    }
    CONFERE(corpoChamado > 0, "folha de Audio vive com estatistica fechada na direita"); }

  // P1: a ilha com um corpo de pe (folha de Audio) e a estatistica na direita
  // escondendo-a NO MESMO QUADRO. Na ordem do app.c a folha pede primeiro e a
  // estatistica esconde depois: um pedido sem corpo (pilula de aviso) nao pode
  // herdar o corpo antigo — a pilula nova nasceria com o corpo de outra folha
  // desenhando embaixo por alguns quadros (rasgo). Sem pedido com corpo no
  // quadro, o ultimo e esquecido na hora.
  estatvivo_abrir();      // reabre no lado esquerdo (o lado ficou 1): ilha livre
  frame(10);
  { PlrIlhaPedido p, pill;
    memset(&p, 0, sizeof p);
    p.w = 720; p.h = 400; p.corpo = corpoFake;
    memset(&pill, 0, sizeof pill);
    pill.texto = "Aviso"; pill.aberta = 1;   // so a pilula, como o aviso da legenda
    // Assenta o corpo A com o painel do outro lado.
    ajustes_definir_estat_lado(1);
    corpoChamado = 0;
    for (i = 0; i < 200 && !corpoChamado; i++) {
      plrilha_pedir(&p);
      estatvivo_desenhar(SDL_GetTicks());
      plrilha_desenhar(SDL_GetTicks());
      SDL_Delay(16);
    }
    CONFERE(corpoChamado > 0, "corpo A assentou");
    corpoChamado = 0;
    // O quadro da troca: pedido so de pilula + esconder, na ordem do app.c.
    // O corpo A nao pode desenhar nem aqui nem quando a ilha volta (a pilula
    // ficaria com o corpo da folha antiga embaixo por alguns quadros).
    ajustes_definir_estat_lado(0);
    plrilha_pedir(&pill);
    estatvivo_desenhar(SDL_GetTicks());
    plrilha_desenhar(SDL_GetTicks());
    ajustes_definir_estat_lado(1);
    for (i = 0; i < 8; i++) {
      plrilha_pedir(&pill);
      estatvivo_desenhar(SDL_GetTicks());
      plrilha_desenhar(SDL_GetTicks());
      SDL_Delay(16);
    }
    CONFERE(corpoChamado == 0, "pilula nova nao herda o corpo da folha antiga"); }
  estatvivo_fechar();
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  puts("PASS: estatisticas ao vivo (geometria)");
  return 0;
}
