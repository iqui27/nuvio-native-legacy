/* Real panel renderer: header clearance and selected accent, with PNG contact sheets.
 * Included here to inspect private layout without adding a production test API. */
#ifndef SALVOSTABS_SOURCE
#define SALVOSTABS_SOURCE "../src/salvospainel.c"
#endif
#define recomenda_n proof_recommendations
#define recomenda_n_novas proof_new_recommendations
#define socialvis_n_amigos proof_friends
#include SALVOSTABS_SOURCE
#undef recomenda_n
#undef recomenda_n_novas
#undef socialvis_n_amigos
int proof_recommendations(void) { return 5; }
int proof_new_recommendations(void) { return 0; }
int proof_friends(void) { return 16; }
#include <SDL2/SDL_image.h>
#include <assert.h>
#include "dados.h"

static unsigned char *pixels;
static int failures;
static void check(int ok, const char *what, int scale, int glass, int accent, int lang, int tab) {
  if (!ok) {
    fprintf(stderr, "FAIL %s scale=%d glass=%d accent=%d lang=%d tab=%d\n",
            what, scale, glass, accent, lang, tab);
    failures++;
  }
}
static void frame(void) {
  tex_bombear(32);
  txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
  gfx_sem_recorte();
  glClearColor(.12f, .16f, .22f, 1); glClear(GL_COLOR_BUFFER_BIT);
  spainel_desenhar(0);
  glFinish();
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
}
int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  const int scales[] = {85, 100, 130}, langs[] = {1, 0, 6};
  const char *names[] = {"en", "pt", "de"};
  SDL_Window *window;
  SDL_GLContext context;
  GLuint texture, target;
  int s, g, c, l, t, k;
  assert(argc == 2 && dir);
  dados_iniciar(dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO) == 0);
  IMG_Init(IMG_INIT_PNG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  window = SDL_CreateWindow("Header proof", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(window); context = SDL_GL_CreateContext(window); assert(context);
  glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &target); glBindFramebuffer(GL_FRAMEBUFFER, target);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  { char art[1024]; assert(realpath("deploy/app/art", art)); gfx_icones_dir(art); }
  /* Complete migrations before choosing matrix settings. */
  ajustes_dir(dir);
  pixels = malloc(1920 * 1080 * 4); assert(pixels);
  assert(garantirLinhas(64)); nLinhas = 64; nCont = 12;
  memset(linhas, 0, sizeof(*linhas) * 64);
  for (k = 0; k < 64; k++) snprintf(linhas[k].titulo, sizeof linhas[k].titulo, "Saved title %d", k + 1);
  entrada = entradaP = trocaT = 1; aberto = 1;
  for (s = 0; s < 3; s++) for (g = 0; g < 2; g++) for (c = 0; c < 2; c++) for (l = 0; l < 3; l++) {
    char path[1024]; FILE *settings;
    float scale = scales[s] / 100.0f;
    int width = (int)ceilf(776 * scale), height = (int)ceilf(140 * scale);
    SDL_Surface *sheet = SDL_CreateRGBSurfaceWithFormat(0, width, height * 6, 32, SDL_PIXELFORMAT_RGBA32);
    assert(sheet);
    snprintf(path, sizeof path, "%s/ajustes.txt", dir); settings = fopen(path, "w"); assert(settings);
    fprintf(settings, "performanceAutoVersaoLocal 1\nidioma %d\nidiomaAutoLocal 0\nselected_theme 1\nfonteInterface 3\nvidroLocal %d\nfocoAcentoLocal %d\nescalaTextoLocal %d\n",
            langs[l], !g, !c, s == 0 ? 0 : s == 1 ? 2 : 5);
    fclose(settings); ajustes_dir(dir); ajustes_dir(dir);
    gfx_escala_ui_definir(1); gfx_escala_txt_definir(scale);
    check(ajustes_foco_acento() == (g && c), "existing glass gate", scales[s], g, c, l, -1);
    for (t = 0; t < 6; t++) {
      float previous = gfx_escala_entrar();
      float rowY = SP_ABAS_Y, kickY = SP_KICK_Y;
      TxtLinha mini = txt_linha(TXT_MINI, i18n("Ligue ou desligue as abas e mude a ordem."), 243, 242, 239, 255);
      int x0 = (int)roundf(SP_X * scale), y0 = (int)roundf(SP_Y * scale), y;
      gfx_escala_sair(previous);
      aba = t == 5 ? SP_ABA_SOCIAL : t; editando = t == 5; foco = 0;
      editLapis = 0; pilIni = 0; nAg = 0; nSocial = nAtv = 0;
      check(kickY + mini.h + 12 <= rowY + .01f, "kicker bottom + 12 <= tabs top", scales[s], g, c, l, t);
      for (k = 0; k < 2; k++) { frame(); SDL_Delay(2); }
      if (t != 5) {
        /* Sample inside the moving selection, clear of its icon/count and rim. */
        previous = gfx_escala_entrar();
        int px = (int)roundf((pilX + pilW / 2) * scale);
        int py = (int)roundf((SP_ABAS_Y + SP_SEG_PAD + 6) * scale);
        gfx_escala_sair(previous);
        const unsigned char *p = pixels + ((1079 - py) * 1920 + px) * 4;
        check(!g || !c || p[0] > p[1] + 10, "selected pill accent tint", scales[s], g, c, l, t);
      }
      for (y = 0; y < height; y++)
        memcpy((char *)sheet->pixels + (t * height + y) * sheet->pitch,
               pixels + ((1079 - y0 - y) * 1920 + x0) * 4, width * 4);
    }
    snprintf(path, sizeof path, "%s/%d-glass%d-accent%d-%s.png", argv[1], scales[s], g, c, names[l]);
    assert(IMG_SavePNG(sheet, path) == 0); SDL_FreeSurface(sheet);
  }
  printf("%s: 216 header cases (Saved, Activity, Friends, Notifications, Calendar, Edit); 36 PNG sheets\n", failures ? "FAIL" : "PASS");
  free(pixels); SDL_GL_DeleteContext(context); SDL_DestroyWindow(window); SDL_Quit();
  return failures ? 1 : 0;
}
