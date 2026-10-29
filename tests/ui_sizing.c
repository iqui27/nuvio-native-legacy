// Real Sources layout and renderer: density, bounds, navigation, pixel baseline.
static int island;
#define player_aberto ui_test_player_open
#include "../src/streams.c"
#undef player_aberto
int ui_test_player_open(void) { return island; }
#include "estatvivoui.h"
#include "dados.h"
#include "tex_cache.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <unistd.h>
void ui_settings_test(const char *dir, int baseline);

static int visible(void) {
  int count = 0;
  for (int i = 0; i < nOrdem; i++)
    if (linhaY[i] >= rolagem && linhaY[i] + linhaH[i] <= rolagem + areaLista()) count++;
  return count;
}
static void draw(SDL_Window *window, const char *name) {
  for (int k = 0; k < 90; k++) {
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    stream_folha_atualizar(1.0f / 60, (Uint32)(1000 + k * 16));
    glClearColor(.03f, .03f, .04f, 1); glClear(GL_COLOR_BUFFER_BIT);
    stream_folha_desenhar((Uint32)(1000 + k * 16));
  }
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(s);
  for (int y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, name) == 0);
  free(pixels); SDL_FreeSurface(s);
  SDL_GL_SwapWindow(window);
}
int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "--auto")) {
    setenv("NUVIO_PANEL_INCHES", argv[2], 1);
    assert(fabsf(ajustes_tamanho_ui() - atof(argv[3])) < .001f);
    assert(fabsf(ajustes_tamanho_ui() - atof(argv[3])) < .001f);
    return 0;
  }
  int baseline = argc > 1 && !strcmp(argv[1], "--baseline");
  const char *out = getenv("UI_SIZING_OUT"); assert(out);
  const char *dir = getenv("NUVIO_DADOS"); assert(dir);
  setenv("NUVIO_PANEL_INCHES", "85", 1);
  dados_iniciar(dir);
  ui_settings_test(dir, baseline);
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *window = SDL_CreateWindow("UI sizing", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(window);
  SDL_GLContext gl = SDL_GL_CreateContext(window); assert(gl);
  gfx_tamanho_alvo(1920, 1080); glViewport(0, 0, 1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art"); badges_carregar("deploy/app/art");
  Stream sources[16] = {0};
  for (int i = 0; i < 16; i++) {
    sources[i].mp4 = 1; sources[i].fileIdx = -1; sources[i].altura = 1080;
    snprintf(sources[i].url, sizeof sources[i].url, "https://media.invalid/%d.mp4", i);
    snprintf(sources[i].provedor, sizeof sources[i].provedor, "Addon");
    snprintf(sources[i].rotulo, sizeof sources[i].rotulo, "Source %d", i + 1);
  }
  stream_definir_lista(sources, 16);
  gfx_escala_ui_definir(1);
  stream_folha_abrir();
  char png[1024]; snprintf(png, sizeof png, "%s/sources-100.png", out); draw(window, png);
  int rows100 = visible();
  assert(rows100 > 0 && FOLHA_W == 820 && NV_VTELA_W == 1920 && NV_VTELA_H == 1080);
  if (!baseline) {
    gfx_escala_ui_definir(.85f); assert(gfx_escala_ui() == .85f);
    stream_folha_abrir();
    snprintf(png, sizeof png, "%s/sources-85.png", out); draw(window, png);
    int rows85 = visible(); assert(rows85 > rows100);
    assert(FOLHA_TOPO * .85f >= 0 && folhaBase * .85f < 1080);
    float x = NV_TELA_W - FOLHA_W - FOLHA_MARGEM;
    assert(x * .85f >= 0 && (x + FOLHA_W) * .85f <= 1920);
    for (int i = 0; i < nOrdem; i++) {
      foco = i; grupo = 1; rolagem = alvoRolagem();
      float y = FOLHA_TOPO + linhaY[i] - rolagem;
      assert(y >= FOLHA_TOPO - .01f && y + linhaH[i] <= folhaBase + .01f);
    }
    // Player island uses a different header offset and bottom margin.
    island = 1; folhaGeometria();
    assert(FOLHA_TOPO == 308 && folhaBase * .85f < 1080);
    for (int i = 0; i < nOrdem; i++) {
      foco = i; rolagem = alvoRolagem();
      float y = FOLHA_TOPO + linhaY[i] - rolagem;
      assert(y >= FOLHA_TOPO - .01f && y + linhaH[i] <= folhaBase + .01f);
    }
    island = 0;
    float stats[4]; estatvivo_retangulo(EV_TAM_COMPLETO, 0, stats);
    assert((stats[0] + stats[2]) * .85f <= 1920 && (stats[1] + stats[3]) * .85f < 1080);
    printf("Sources: %d full rows at 100%%, %d at 85%%; every focused row fits, including player island\n", rows100, rows85);
    gfx_escala_sair(.8f);
    TxtLinha meta = txt_linha(TXT_HERO_META, "metadata", 255, 255, 255, 255);
    assert(meta.h * .8f >= 18); // Physical 1080p-equivalent floor.
    assert(txt_linha_corta(TXT_HERO_META, "Long metadata must fit within the available width", 255, 255, 255, 255, 220).w <= 220);
    // Switching to legacy scale must not reuse the floored text texture.
    gfx_escala_ui_definir(1); txt_novo_quadro();
    TxtLinha legacy = txt_linha(TXT_HERO_META, "metadata", 255, 255, 255, 255);
    assert(legacy.w < meta.w);
    gfx_escala_ui_definir(.8f); txt_novo_quadro();
    assert(txt_linha(TXT_HERO_META, "metadata", 255, 255, 255, 255).w == meta.w);
    gfx_escala_sair(1);
  }
  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(window); SDL_Quit();
  puts("ui_sizing: PASS"); return 0;
}
