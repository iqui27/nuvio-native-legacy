/* Offline remote navigation and decoder routing. All sources are fixtures;
 * simulated ready states never establish physical decoder capacity. */
#include "plrui.h"
#include "epg.h"
#include "video.h"
static int host_extra_decoders(void) { return video_mv_max(); }
/* The production overlay starts EPG refresh; keep this fixture offline. */
static void offline_epg_start(void) {}
#define epg_iniciar offline_epg_start
static int slot_focus_surfaces;
static void watch_focus_surface(GfxRect r, float radius, float alpha);
#define plrui_linha_foco watch_focus_surface
#define main account_regression_main
#include "guiav5_multiview.c"
#undef main
#undef epg_iniciar
#undef plrui_linha_foco
#include <SDL2/SDL_image.h>
#include <sys/stat.h>

static void watch_focus_surface(GfxRect r, float radius, float alpha) {
  slot_focus_surfaces++;
  plrui_linha_foco(r, radius, alpha);
}
static int failures;
#define CHECK(ok, message) do { if (!(ok)) { puts("FAIL: " message); failures++; } } while (0)
static void key(SDL_Keycode k) {
  SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = k;
  g5ui_event(&e);
}
static void frame(void) {
  now_ms += 50;
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(8); gfx_novo_quadro();
  glClearColor(.045f, .047f, .051f, 1); glClear(GL_COLOR_BUFFER_BIT);
  g5ui_draw();
}
static int visible_focus(int id) {
  for (int i = 0; i < num_focus; i++) if (next_focus[i] == id)
    return focus_rect[i].y >= 364 && focus_rect[i].y + focus_rect[i].h <= 900;
  return 0;
}
static void capture(const char *dir, const char *name) {
  char path[1024]; snprintf(path, sizeof path, "%s/%s.png", dir, name);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(s && pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y*s->pitch, pixels + (1079-y)*1920*4, 1920*4);
  assert(IMG_SavePNG(s, path) == 0); free(pixels); SDL_FreeSurface(s);
}
static void many_channels(void) {
  stagingZerar(); sTeto = 30000;
  for (int f = 0; f < 5; f++) {
    char source[24]; snprintf(source, sizeof source, "m%08d", f);
    int idx = sFonteChave(source, LTV_M3U, source, "");
    for (int n = 0; n < 9; n++) {
      char id[80], name[80]; snprintf(id, sizeof id, "m3u:%s:%d", source, n);
      snprintf(name, sizeof name, "Fixture channel %02d", f*9+n+1);
      canal(idx, id, name, "http://fixture.invalid/live.m3u8", "", "");
    }
  }
  favLido = 1; adotarStaging(); estado = G_PRONTO;
}
static void picker_navigation(const char *out) {
  g5ui_open("multiview-4"); mv_target = 1; g5ui_open("multiview-escolher");
  frame(); key(SDLK_DOWN); frame();
  int lost = 0, clipped = 0;
  for (int i = 0; i < npicks; i++) if (!picks[i].header) {
    if (focus != 900+i) { lost++; focus = 900+i; }
    frame();
    if (focus != 900+i) lost++;
    if (!visible_focus(900+i)) clipped++;
    key(SDLK_DOWN);
  }
  CHECK(!lost, "picker must retain focus through 45 channels and source headers");
  CHECK(!clipped, "picker must scroll every focused row fully inside viewport");
  focus = 900+npicks-1; frame();
  capture(out, "picker-last-local-1080p");
  for (int i = npicks-1; i >= 0; i--) if (!picks[i].header) {
    CHECK(focus == 900+i, "picker Up must cross source headers");
    frame(); key(SDLK_UP);
  }
  CHECK(focus == 899, "picker Up returns to search");
  frame(); key(SDLK_LEFT); frame();
  CHECK(focus == 899 && nav == -1, "picker must contain horizontal navigation");
  key(SDLK_ESCAPE);
  CHECK(!strcmp(screen, "multiview-4"), "picker Back restores layout");
  G5Item keep = mv_items[0];
  for (int target = -1; target <= mode; target += mode+1) {
    g5ui_open("multiview-escolher"); mv_target = target;
    activate(901);
    CHECK(!memcmp(&keep, &mv_items[0], sizeof keep), "stale picker target must not overwrite slot zero");
    key(SDLK_ESCAPE);
  }
}
static void guide_navigation(const char *out) {
  g5ui_open("grade"); gv_fonte[0] = gv_grupo[0] = 0;
  gv_lin = gv_off = gv_top = 0; gv_zone = 0; gv_snap_force = 1; gv_snapshot();
  for (int n = 1; n < 45; n++) {
    key(SDLK_DOWN); frame();
    CHECK(gv_lin == n && gv_focus(), "Guide Down must cross copied channel windows");
  }
  CHECK(gv_top > 0 && gv_off > 0, "Guide follows focus while scrolling");
  for (int n = 0; n < 60; n++) frame();
  CHECK(fabsf(gv_rol-gv_top) < .02f, "Guide scroll spring must settle on focused window");
  CHECK(gv_rows_y(GV_BASE) + (gv_lin-gv_top)*(GV_ROW_H+GV_ROW_GAP) + GV_ROW_H <= GV_GRID_BOTTOM, "Guide final focused row must fit inside viewport");
  capture(out, "guide-last-local-1080p");
  key(SDLK_DOWN); CHECK(gv_zone == 4, "Guide last row reaches source chips");
  key(SDLK_UP); CHECK(gv_zone == 0, "Guide source chips return to channels");
  key(SDLK_ESCAPE); CHECK(gv_side_open, "Guide Back opens sidebar");
  key(SDLK_RIGHT); CHECK(!gv_side_open, "Guide Right leaves sidebar");
  /* Live's Down/Blue mini-guide uses the same published channel inventory.
   * Its own key handling is exercised without opening a player or network. */
  guia_fechar(); fontesOk = 1; guia_overlay_abrir();
  focoLin = focoCol = 0; now_ms += G_OVERLAY_REP_MS + 1;
  for (int n = 0; n < 35; n++) {
    GCanal *before = linhaItem(focoLin, focoCol);
    SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = SDLK_DOWN;
    guia_evento(&e);
    CHECK(linhaItem(focoLin, focoCol) && linhaItem(focoLin, focoCol) != before, "Live mini-guide Down advances beyond eight channels");
  }
  { SDL_Event e = {.type = SDL_KEYDOWN}; e.key.keysym.sym = SDLK_ESCAPE; guia_evento(&e); }
  CHECK(!guia_overlay_aberta(), "Live mini-guide Back closes overlay");
  guia_fechar();
}
static void routing(void) {
  go("multiview-4"); mode = 4;
  video_mv_parar_todos(); memset(&mvctl, 0, sizeof mvctl); memset(mv_extra_on, 0, sizeof mv_extra_on);
  memset(opens, 0, sizeof opens); memset(last_url, 0, sizeof last_url);
  for (int main_slot = 0; main_slot < 4; main_slot++) {
    mv_main = main_slot; decoder_limit = 3;
    for (int i = 0; i < 4; i++) {
      memset(&mv_items[i], 0, sizeof mv_items[i]);
      snprintf(mv_items[i].id, sizeof mv_items[i].id, "slot-%d", i);
      snprintf(mv_items[i].name, sizeof mv_items[i].name, "Fixture %d", i);
      snprintf(mv_items[i].url, sizeof mv_items[i].url, "http://fixture.invalid/%d", i);
      snprintf(mv_items[i].poster, sizeof mv_items[i].poster, "design/glass-ilha/arte.jpg");
      snprintf(mv_fonte[i], sizeof mv_fonte[i], "mfixture");
    }
    tick(2);
    for (int i = 0; i < 4; i++) if (i != main_slot) {
      int decoder = i-(i > main_slot);
      CHECK(opened[decoder] && !strcmp(last_url[decoder], mv_items[i].url), "UI slots must map to dense extra decoder indices");
      GfxRect r = mv_rect(i), actual = decoder_rect[decoder];
      CHECK(actual.x == (int)r.x && actual.y == (int)r.y && actual.w == (int)r.w && actual.h == (int)r.h, "decoder windows must follow their UI slot");
    }
    for (int limit = 0; limit <= 3; limit++) {
      decoder_limit = limit; tick(2);
      for (int i = 0; i < 4; i++) if (i != main_slot) {
        int decoder = i-(i > main_slot);
        CHECK(mv_disp[i] == (decoder < limit ? G5_MV_TOCANDO : G5_MV_SEM_DECODER), "software capacity 0..3 extras must gate and recover each slot");
        CHECK(opened[decoder] == (decoder < limit), "capacity changes must stop/reopen the mapped decoder");
      }
    }
  }
  CHECK(g5_mv_source_cap("xt", -1) == 1 && g5_mv_source_cap("xta00000003", 2) == 2, "Xtream limits stay account scoped");
  CHECK(g5_mv_source_cap("st", 9) == 1 && g5_mv_source_cap("sta00000001", 9) == 1, "Stalker uses one connection");
  CHECK(g5_mv_source_cap("mfixture", 1) == -1 && g5_mv_source_cap("afixture", 1) == -1, "M3U/addon have no software account cap");
}
static void slot_focus(const char *out) {
  go("multiview-4"); nav = -1; focus = 0; sem_foco = 0; gv_dt = 0;
  for (int glass = 0; glass <= 1; glass++) {
    ajustes_definir_vidro(glass);
    slot_focus_surfaces = 0; mv_slot(0);
    CHECK(slot_focus_surfaces == 0, "focused slot must never paint gray focus surface over video/art");
  }
  /* Still art in every slot: a local rendering fixture, never live proof. */
  memset(mv_extra_on, 0, sizeof mv_extra_on); memset(mv_born, 0, sizeof mv_born);
  GLuint art = 0;
  for (int n = 0; n < 200 && !art; n++) {
    frame(); SDL_Delay(10);
    art = tex_obter_larg(mv_items[0].poster, mv_rect(0).w);
  }
  CHECK(art != 0, "local slot fixture art must finish uploading before capture");
  frame(); capture(out, "slot-focus-local-1080p");
  key(SDLK_RIGHT); frame(); CHECK(focus == 1, "Multiview Right reaches adjacent slot");
  key(SDLK_DOWN); frame(); CHECK(focus == 3, "Multiview Down reaches lower slot");
  key(SDLK_UP); frame(); CHECK(focus == 1, "Multiview Up returns to upper slot");
  key(SDLK_UP); frame(); CHECK(focus >= 800 && focus <= 806, "Multiview Up reaches controls");
}
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--accounts")) return account_regression_main();
  const char *out = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/21-guide-live-mv-followup";
  mkdir(out, 0755);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0); IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Offline TV navigation fixtures", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1)); tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS")); ajustes_dir(getenv("NUVIO_DADOS")); context();
  CHECK(host_extra_decoders() == 0, "desktop backend exposes zero extra pipelines, not measured hardware capacity");
  many_channels(); picker_navigation(out); guide_navigation(out); routing(); slot_focus(out);
  tex_encerrar(); txt_encerrar(); gfx_encerrar(); SDL_DestroyWindow(window); SDL_Quit();
  printf("%s: offline focus/navigation, source limits, simulated decoder mapping; hardware unverified\n", failures ? "FAIL" : "PASS");
  return failures ? 1 : 0;
}
