/* Headless regression of the real guide UI, preview requests and player handoff. */
#include "../src/guia.c"
#include "tela21.h"
TxtLinha test_source_text(TxtEstilo, const char *, int, int, int, int, float);
#define txt_linha_corta_bruta test_source_text
#define txt_desenhar(l,x,y) ((void)0)
#define t21_ilha(...) ((void)0)
#define gfx_icone(...) ((void)0)
#define plrui_botao_repouso(...) ((void)0)
#define plrui_pilula_foco(...) ((void)0)
#ifndef GUIAV5_UNDER_TEST
#define GUIAV5_UNDER_TEST "../src/guiav5ui.c"
#endif
#include GUIAV5_UNDER_TEST
#include <assert.h>

static int failures;
#define CHECK(ok, name) do { if (!(ok)) { fprintf(stderr, "FAIL: %s\n", name); failures++; } else puts("PASS: " name); } while (0)

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(getenv("NUVIO_DADOS"));
  context();
  stagingZerar(); sTeto = 30000;
  for (int i = 0; i < 2; i++) {
    int f = sFonte(i ? LTV_ADDON : LTV_M3U, i ? "addon" : "m3u", i ? "Live addon" : "IPTV playlist", "");
    GCru c = {0};
    c.id = i ? "channel-b" : "channel-a"; c.nome = i ? "Channel B" : "Channel A";
    c.grupo = "News"; c.logo = c.desc = c.epgId = c.tvgNome = c.url = c.ua = c.ref = "";
    sGrupoInfo(f, c.grupo, 1); sCanalAdd(f, &c); sLtvF[f].total++;
  }
  favLido = 1; adotarStaging(); estado = G_PRONTO; aberta = 1;
  snprintf(screen, sizeof screen, "cartoes");
  snprintf(gv_pagina, sizeof gv_pagina, "cartoes");
  gv_lin = 1; gv_top = 1; gv_card = 1; gv_zone = 6; focus = 42;
  /* Observe the actual labels emitted by Manage sources. */
  g5ui_open("fontes-gerenciar");
  /* The text boundary records labels even without a font or GL context. */
  sources_screen();
  extern int saw_iptv, saw_addon, saw_empty;
  CHECK(saw_iptv && saw_addon && !saw_empty, "manage lists both live sources without a v5 draft");
  gv_voltar();
  gv_assistir("channel-b");
  CatItem it = {0};
  int preview = guia_pediu_preview(&it);
  CHECK(preview && !strcmp(it.imdb, "channel-b") && !pediuCanal, "first OK requests preview only");
  float x,y,w,h;
  guia_preview_rect(&x,&y,&w,&h);
  CHECK(x == GV_MX+GV_MW-358 && y == GV_BASE+16 && w == 330 && h == 176,
        "Cards native window matches the drawn preview");
  /* Simulate the app accepting that request using the real player state. */
  player_mini_no_guia(x, y, w, h);
  player_manter_mini();
  CatItem tv = {.tipo="channel", .imdb="channel-b", .titulo="Channel B"};
  cat_definir(&tv, 1); player_abrir(0, NULL); player_marcar_canal(&tv);
  static const VideoSimulacao sim = {.aceita=1, .pronto=1, .tocando=1};
  CHECK(!guia_preview_no_ar(), "idle preview remains dark until a video frame is ready");
  video_simular(&sim); player_definir_fonte("https://example.invalid/live.m3u8");
  CHECK(guia_preview_no_ar(), "accepted preview source reveals video while guide stays open");
  pediuCanal = 0;
  gv_assistir("channel-b");
  CHECK(guia_pediu_restaurar() && !guia_pediu_preview(&it) && !pediuCanal && g5ui_active(), "second OK restores same stream");
  player_restaurar();
  player_minimizar_para_guia(100, 100, 800, 450);
  guia_adotar_canal("channel-b");
  CHECK(g5ui_active() && !strcmp(screen, "cartoes") && gv_lin == 1 && gv_top == 1 && gv_card == 1 && gv_zone == 6 && focus == 42,
        "Back keeps v5 page, scroll and channel focus");
  player_encerrar();
  snprintf(screen,sizeof screen,"grade");snprintf(gv_pagina,sizeof gv_pagina,"grade");
  gv_hh=208;guia_preview_rect(&x,&y,&w,&h);
  CHECK(fabsf(w/h-16.0f/9)<.001f && h==172,
        "compact guide exports its resized preview geometry");
  gv_hh=GV_HERO_H;guia_preview_rect(&x,&y,&w,&h);
  CHECK(fabsf(w/h-16.0f/9)<.001f && h==GV_HERO_H-36,
        "expanded guide exports its preview geometry");
  SDL_Quit();
  return failures ? 1 : 0;
}

/* Capture rendered text; everything else uses production code. */
#undef txt_linha_corta_bruta
int saw_iptv, saw_addon, saw_empty;
TxtLinha test_source_text(TxtEstilo e, const char *s, int r, int g, int b, int a, float w) {
  (void)e; (void)r; (void)g; (void)b; (void)a; (void)w;
  if (!strcmp(s, "IPTV playlist")) saw_iptv = 1;
  if (!strcmp(s, "Live addon")) saw_addon = 1;
  if (!strcmp(s, "Nenhuma fonte")) saw_empty = 1;
  return (TxtLinha){0};
}
