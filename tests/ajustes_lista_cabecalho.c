// Mobile "Lista": compare production header, badge and clock rectangles.
#include "../src/ajustes.c"
#include "../src/menu.h"
#include "../src/ilha.h"
#include <assert.h>
#include <SDL2/SDL_image.h>

static void capture(SDL_Window *window, int width, int height, int scale) {
  const char *directory = getenv("NUVIO_LISTA_PNG_DIR");
  if (!directory) return;
  focarSecao(0);
  focoIndice = 0;
  for (int frame = 0; frame < 120; frame++) {
    aj2Atualizar(1.0f / 60.0f);
    gfx_novo_quadro();
    txt_novo_quadro();
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(1000 + frame * 16);
    menu_pilula_mostrar(1.0f);
    menu_desenhar(1000 + frame * 16);
    ilha_posicionar(1);
    ilha_desenhar(1000 + frame * 16);
    if (frame == 119) {
      char path[1024];
      unsigned char *pixels = malloc((size_t)width * height * 4);
      SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
      assert(pixels && surface);
      glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < height; y++)
        memcpy((char *)surface->pixels + y * surface->pitch,
               pixels + (size_t)(height - 1 - y) * width * 4, width * 4);
      snprintf(path, sizeof path, "%s/lista-%dx%d-%d.png", directory, width, height, scale);
      assert(IMG_SavePNG(surface, path) == 0);
      SDL_FreeSurface(surface);
      free(pixels);
    }
    SDL_GL_SwapWindow(window);
  }
}

static GfxRect pixels(GfxRect r, float scale) {
  return (GfxRect){r.x * scale, r.y * scale, r.w * scale, r.h * scale};
}

static int intersects(GfxRect a, GfxRect b) {
  return a.x < b.x + b.w && b.x < a.x + a.w &&
         a.y < b.y + b.h && b.y < a.y + a.h;
}

int main(void) {
  const int resolutions[][2] = {{1280, 720}, {1920, 1080}, {720, 1280}, {1080, 1920}};
  const float scales[] = {0.85f, 1.0f};
  SDL_Window *window;
  SDL_GLContext context;
  int cases = 0;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Lista regression", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window);
  context = SDL_GL_CreateContext(window);
  assert(context);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_recursos("deploy/app/art");
  ajustes_dir(getenv("NUVIO_DADOS"));
  memcpy(valor, valorPadrao, sizeof valor);
  valor[AJ_HOME_LAYOUT] = HOME_LAYOUT_DINAMICA;
  valor[AJ_LAYOUT_AJUSTES] = 1;
  valor[AJ_VIDRO] = 1; // Glass off, including the C9 appearance path.
  valor[AJ_TAMANHO_AJUSTES] = 2; // Settings 100%; global UI scale varies below.
  gfx_escala_txt_definir(1.0f);
  menu_definir_destino(MENU_AJUSTES);
  ilha_relogio_visivel(1);
  layout_modo_definir(1);
  montarTela();
  assert(nSecoes > 0);
  for (int r = 0; r < 4; r++) {
    layout_tela_definir(resolutions[r][0], resolutions[r][1]);
    SDL_SetWindowSize(window, resolutions[r][0], resolutions[r][1]);
    SDL_PumpEvents();
    gfx_tamanho_alvo(resolutions[r][0], resolutions[r][1]);
    glViewport(0, 0, resolutions[r][0], resolutions[r][1]);
    int width, height, targetWidth, targetHeight;
    GLint viewport[4];
    SDL_GL_GetDrawableSize(window, &width, &height);
    gfx_tamanho_tela(&targetWidth, &targetHeight);
    glGetIntegerv(GL_VIEWPORT, viewport);
    assert(width == resolutions[r][0] && height == resolutions[r][1]);
    assert(targetWidth == width && targetHeight == height);
    assert(viewport[2] == width && viewport[3] == height);
    for (int s = 0; s < 2; s++) {
      gfx_escala_ui_definir(scales[s]);
      assert(gfx_escala_ui() == scales[s]);
      assert(ajustes_layout_lista());
      capture(window, width, height, (int)lroundf(scales[s] * 100));
      GfxRect badge, clock;
      assert(menu_pilula_rect(&badge.x, &badge.y, &badge.w, &badge.h));
      // Settle the real clock draw, whose public rectangle is in its UI canvas.
      for (int frame = 0; frame < 120; frame++) {
        txt_novo_quadro();
        ilha_posicionar(1);
        ilha_desenhar(1000 + frame * 16);
      }
      assert(ilha_rect(&clock.x, &clock.y, &clock.w, &clock.h));
      clock = pixels(clock, gfx_escala_ui());
      assert(badge.w > 0 && badge.h > 0 && clock.w > 0 && clock.h > 0);
      for (int category = 0; category < nSecoes; category++) {
        focarSecao(category);
        assert(secAtual == category);
        GfxRect header = pixels(aj2PaginaR(), ajustes_tamanho_ajustes());
        GfxRect list = pixels(aj2ListaR(), ajustes_tamanho_ajustes());
        assert(!intersects(header, badge));
        assert(!intersects(header, clock));
        assert(list.y >= header.y + header.h);
        // Negative control: the pre-fix Mobile origin must trigger this oracle.
        GfxRect before = header;
        before.y = 0;
        assert(intersects(before, badge) || intersects(before, clock));
        cases++;
      }
    }
  }
  // TV keeps its previous top reservation for both Settings layouts.
  layout_modo_definir(0);
  for (int layout = 0; layout < 2; layout++) {
    valor[AJ_LAYOUT_AJUSTES] = layout;
    assert(fabsf(ajPaginaTopo() * ajustes_tamanho_ajustes() - 112) < 0.001f);
  }
  assert(cases == 8 * nSecoes);
  printf("PASS: %d resolution/UI-scale/category cases; pre-fix overlap detected\n", cases);
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
