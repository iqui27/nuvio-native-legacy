/* Offline presentation regression: real renderer, fixture feeds, no input events. */
#include "text.h"
static TxtLinha visual_line(TxtEstilo, const char *, int, int, int, int, float);
static void visual_text(TxtLinha, float, float);
#define txt_linha_corta_bruta visual_line
#define txt_desenhar visual_text
#define SPORTS_V5_MAIN unused_sports_main
#include "esportes_v5_shot.c"
#undef SPORTS_V5_MAIN
#undef txt_desenhar
#undef txt_linha_corta_bruta

static GLuint visual_error_tex, visual_loading_tex;
static GfxRect visual_error;
static int visual_error_seen, visual_loading_seen;
static TxtLinha visual_line(TxtEstilo style, const char *text, int r, int g, int b, int a, float width) {
  TxtLinha line = txt_linha_corta_bruta(style, text, r, g, b, a, width);
  if (!strcmp(text, i18n("Não foi possível atualizar os eventos"))) visual_error_tex = line.tex;
  if (!strcmp(text, i18n("Carregando…"))) visual_loading_tex = line.tex;
  return line;
}
static void visual_text(TxtLinha line, float x, float y) {
  if (line.tex && line.tex == visual_error_tex) {
    visual_error = (GfxRect){x, y, line.w, line.h}; visual_error_seen++;
  }
  if (line.tex && line.tex == visual_loading_tex) visual_loading_seen++;
  txt_desenhar(line, x, y);
}
static void visual_capture(SDL_Window *window, const char *out, const char *name) {
  visual_error_seen = visual_loading_seen = 0;
  capture(window, out, name);
}
int main(int argc, char **argv) {
  assert(argc == 2 && getenv("NUVIO_DADOS"));
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("Sports visual proof", 0, 0, 1920, 1080,
    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window && SDL_GL_CreateContext(window));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art");
  dados_iniciar(getenv("NUVIO_DADOS"));
  ajustes_definir_vidro(0); context(); semear(); fixture();

  g5ui_open("esportes-ligas"); quadro(); focus = 3000;
  visual_capture(window, argv[1], "hub-solid");
  ajustes_definir_vidro(1); visual_capture(window, argv[1], "hub-glass"); ajustes_definir_vidro(0);
  gs_event_open(&eventos[0]); visual_capture(window, argv[1], "event-live");
  gs_back(); assert(focus == 3000);
  gs_event_open(&eventos[2]); visual_capture(window, argv[1], "event-upcoming"); gs_back();

  g5ui_open("esportes-liga"); gs_league = gs_sport = 0; quadro(); focus = 2203;
  assert(gs_n >= 6);
  esp_fixture_error = 1;
  visual_capture(window, argv[1], "cached-error-solid");
  /* Status must fit between the action row and cards, never cover cached games. */
  int status_clear = visual_error_seen && visual_error.y >= 372 &&
    visual_error.y + visual_error.h <= 398 && visual_error.x >= GV_MX &&
    visual_error.x + visual_error.w <= GV_MX + GV_MW;
  assert(focus == 2203 && gs_n >= 6);
  ajustes_definir_vidro(1); visual_capture(window, argv[1], "cached-error-glass"); ajustes_definir_vidro(0);
  status_clear = status_clear && visual_error_seen && visual_error.y >= 372 &&
    visual_error.y + visual_error.h <= 398;
  focus = 2200; visual_capture(window, argv[1], "cached-error-lifted");
  status_clear = status_clear && visual_error_seen && visual_error.y >= 372 &&
    visual_error.y + visual_error.h <= 398 && focus == 2200;

  nEventos = 0; esp_fixture_revision++; esp_fixture_error = 0; esp_fixture_busy = 1;
  visual_capture(window, argv[1], "loading");
  int loading_label = visual_loading_seen > 0;
  assert(!gs_n);
  esp_fixture_busy = 0; esp_fixture_error = 1; focus = 2034;
  visual_capture(window, argv[1], "error-focused");
  int retry_visible = 0;
  for (int i = 0; i < num_focus; i++) if (next_focus[i] == 2034) retry_visible = 1;
  assert(retry_visible && focus == 2034);
  esp_fixture_error = 0; focus = 2020; visual_capture(window, argv[1], "empty");

  g5ui_open("esportes-ligas"); nNoticias = 0; noticiasEstado = 0;
  esp_fixture_revision++; quadro(); focus = 5000;
  visual_capture(window, argv[1], "news-loading");
  loading_label = loading_label && visual_loading_seen > 0;
  noticiasEstado = -1; visual_capture(window, argv[1], "news-error-focused");
  assert(focus == 5000);
  noticiasEstado = 1; visual_capture(window, argv[1], "news-empty");

  fprintf(stderr, "Sports visual: cached status clear=%d; loading label=%d\n", status_clear, loading_label);
  assert(status_clear && "Refresh status overlaps cached match cards");
  assert(loading_label && "Loading needs a readable status alongside skeletons");
  puts("PASS Sports visual: cached error bounds, loading labels, retry focus, event return; local 1080p only, no input events");
  return 0;
}
