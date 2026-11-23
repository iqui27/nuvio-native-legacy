// NOMEFONTE directed (Mac only — streams_desktop.inc compiles under
// NV_DESKTOP_UI && __APPLE__): the desktop source sheet never draws
// s->descricao, so the focused row must rasterize the WHOLE filename
// (arquivoDa) and reserve its height. A short description that already
// contains the name (the "[Cloud] Movie.mkv" fixture) makes the TV dedupe
// arquivoVisivel return "" — fine on TV, where the description line shows
// it, a bug on desktop, where nothing does.
#include "badges.h"
#include "text.h"
static TxtLinha checkedTxtLinha(TxtEstilo estilo, const char *s, int r, int g, int b, int a);
#define txt_linha checkedTxtLinha
#include "../src/streams.c"
#undef txt_linha
#include <assert.h>

static Uint32 now = 100000;
// arquivoDesenhar is the only caller that rasterizes TXT_PG_FIM 125/125/123:
// everything it pieces together in a frame is the filename being drawn.
static char drawn[4096];
static size_t drawnLen;
static TxtLinha checkedTxtLinha(TxtEstilo estilo, const char *s, int r, int g, int b, int a) {
  if (r == 125 && g == 125 && b == 123 && a == 255)
    drawnLen += snprintf(drawn + drawnLen, sizeof drawn - drawnLen, "%s", s);
  return txt_linha(estilo, s, r, g, b, a);
}
void fonte_addon_measure(void) {}
void fonte_addon_box(int x, int y, int w, int h, int emoji) { (void)x; (void)y; (void)w; (void)h; (void)emoji; }
void fonte_addon_visible(const char *text, float y, float h) { (void)text; (void)y; (void)h; }
void fonte_addon_line(const char *text, float y, float w, float h, float limit) {
  (void)text; (void)y; (void)w; (void)h; (void)limit;
}

static void frame(void) {
  now += 16; SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(16);
  stream_folha_atualizar(.016f, now);
  gfx_novo_quadro();
  stream_folha_desenhar(now);
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("nomefonte", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win && SDL_GL_CreateContext(win));
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); assert(tex_iniciar(64));
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS")); ajustes_iniciar();
  ajustes_shot_valor("relogioTelaLocal", 0);

  // Desktop window: layout_desktop_ativo() makes folhaDesktop() true and the
  // sheet takes the folhaDesktopDesenhar path (streams_desktop.inc).
  layout_tela_definir(2880, 1800); layout_janela_definir(1440, 900);
  assert(folhaDesktop() && "wide desktop window must take the desktop sheet");

  AddonRemoto addon = {.nome = "AIOStreams", .url = "https://example.invalid/manifest.json", .ativo = 1};
  addons_definir_lista(&addon, 1);

  // The r1 fixture: description short and already containing the filename —
  // arquivoVisivel dedupes it to "" (TV shows it on the description row;
  // desktop has no such row).
  static Stream s;
  memset(&s, 0, sizeof s);
  snprintf(s.arquivo, sizeof s.arquivo, "[Cloud] Movie.mkv");
  snprintf(s.descricao, sizeof s.descricao, "☁️ Movie.mkv");
  snprintf(s.rotulo, sizeof s.rotulo, "AIOStreams Movie");
  snprintf(s.provedor, sizeof s.provedor, "AIOStreams");
  snprintf(s.url, sizeof s.url, "https://example.invalid/movie.mkv");
  s.altura = 2160; s.tamanhoMB = 9000; s.fileIdx = -1;
  const float aw = FOLHA_W - FOLHA_PAD_E - FOLHA_PAD_D - 2 * FOLHA_TXT;
  assert(!strcmp(arquivoDa(&s), "[Cloud] Movie.mkv"));
  assert(!arquivoVisivel(&s, aw)[0] && "fixture must take the dedupe-hidden path");

  stream_definir_lista(&s, 1);
  stream_folha_abrir();
  for (int i = 0; i < 120; i++) frame();   // sheet + focus spring settle

  drawn[0] = 0; drawnLen = 0; frame();
  assert(!strcmp(drawn, "[Cloud] Movie.mkv") &&
         "desktop row must rasterize the full filename, not the deduped one");

  // The opening row must reserve exactly the drawn name's height (plus the
  // StreamFit lines the TV/desktop share), not the height of "".
  float want = arquivoAltura("[Cloud] Movie.mkv", aw) + FOLHA_FIT_H * fitLinhas(0);
  float got = alturaLinha(0, automaticaDaFolha(), 1) - alturaLinha(0, automaticaDaFolha(), 0);
  assert(fabsf(got - want) < .5f && "focused desktop row must reserve the filename height");

  printf("PASS: desktop sheet draws and reserves the full filename \"%s\"\n", drawn);
  return 0;
}
