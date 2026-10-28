// Search: gap under the top menu pill at every text scale, and the ?!# page
// typing accents and punctuation (UTF-8) into the live query.
#include <assert.h>
#include <stdio.h>
#include <SDL2/SDL_image.h>
// The pill sits at fixed REAL px (what the TCL shows at 80%), the Search screen
// measures in virtual px (real / text scale).
static int pilula_falsa(float *x, float *y, float *w, float *h) {
  *x = 34; *y = 35; *w = 150; *h = 45;
  return 1;
}
#define menu_pilula_rect pilula_falsa
#ifndef BUSCA_C
#define BUSCA_C "../src/busca.c"
#endif
#include BUSCA_C

#define GAP_MIN 20.0f
static int busca_teclado_gap(void) {
  static const float escalas[] = { 0.8f, 1.0f, 1.3f };
  int bad = 0;
  for (int i = 0; i < 3; i++) {
    float x, y, w, h, gap;
    gfx_escala_txt_definir(escalas[i]);
    TXT_INI();
    pilula_falsa(&x, &y, &w, &h);
    gap = buTopoEsq() - (y + h) / gfx_escala_txt();
    printf("%s gap pilula->campo %.0f%%: %.1f px (min %.0f)\n", gap >= GAP_MIN ? "PASS" : "FAIL",
           escalas[i] * 100, gap, GAP_MIN);
    bad |= gap < GAP_MIN;
    TXT_FIM();
  }
  gfx_escala_txt_definir(1.0f);
  return bad;
}

#ifndef SO_GAP
static void tocar(const char *tecla) {
  for (int f = 0; f < kbFil; f++)
    for (int c = 0; c < KB_COLUNAS[f]; c++)
      if (!strcmp(kbTeclas[f * BU_KB_COLS + c], tecla)) {
        focoKb.fileira = f; focoKb.coluna = c; aplicarTecla(); return;
      }
  printf("FAIL tecla %s ausente\n", tecla); assert(0);
}
static int busca_teclado_simbolos(void) {
  static const char *esperadas[] = { "\xc3\xa1", "\xc3\xa0", "\xc3\xa2", "\xc3\xa3", "\xc3\xa9", "\xc3\xaa",
    "\xc3\xad", "\xc3\xb3", "\xc3\xb4", "\xc3\xb5", "\xc3\xba", "\xc3\xa7", "\xc3\xb1", "\xc3\xbc",
    ".", ",", ":", ";", "'", "\"", "!", "?", "-", "_", "&", "(", ")", "/", "@", "#", "+", "=", "*" };
  layoutCir = pagSim = 0; nConsulta = 0; consulta[0] = 0;
  kbMontar();
  focus_iniciar(&focoKb, kbFil + 1, KB_COLUNAS);
  focoKb.fileira = kbFil; focoKb.coluna = 3; aplicarTecla();     // ?!#
  assert(pagSim == 1);
  for (unsigned i = 0; i < sizeof esperadas / sizeof *esperadas; i++) {
    int ach = 0;
    for (int k = 0; k < kbN; k++) ach |= !strcmp(kbTeclas[k], esperadas[i]);
    if (!ach) { printf("FAIL falta a tecla %s\n", esperadas[i]); return 1; }
  }
  tocar("\xc3\xa9"); tocar(":");
  assert(!strcmp(busca_consulta(), "\xc3\xa9:"));
  { char n[16]; busca_normalizar("Am\xc3\xa9lie", n, sizeof n); assert(!strcmp(n, "amelie")); }
  focoKb.fileira = kbFil; focoKb.coluna = 3; aplicarTecla();     // abc
  assert(pagSim == 0 && kbN == 36 && !strcmp(kbTeclas[0], "a"));
  puts("PASS pagina ?!# digita \"é\" e \":\" (UTF-8) e volta para abc");
  return 0;
}
#endif

static SDL_Window *captureWindow;
static void captureNative(const char *name) {
  const char *dir = getenv("NUVIO_BUSCA_PNGS");
  if (!dir) return;
  if (!captureWindow) {
    assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    captureWindow = SDL_CreateWindow("Native Search regression", 0, 0, 1920, 1080,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
    assert(captureWindow && SDL_GL_CreateContext(captureWindow));
    SDL_GL_SetSwapInterval(0);
    gfx_tamanho_alvo(1920, 1080); glViewport(0, 0, 1920, 1080);
    assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
    char art[1024]; assert(realpath("deploy/app/art", art));
    gfx_icones_dir(art); tex_iniciar(32);
  }
  double elapsed = 0, draw = 0, fill = 0; int rects = 0, full = 0;
  for (int i = 0; i < 180; i++) {
    Uint64 start = SDL_GetPerformanceCounter();
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(12); gfx_novo_quadro();
    busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
    Uint64 drawing = SDL_GetPerformanceCounter();
    busca_desenhar(SDL_GetTicks());
    glFinish();
    double ms = 1000.0 / SDL_GetPerformanceFrequency();
    if (i >= 60) {
      draw += (SDL_GetPerformanceCounter() - drawing) * ms;
      elapsed += (SDL_GetPerformanceCounter() - start) * ms;
      fill += gfx_fill; rects += gfx_n_rect; full += gfx_n_cheio;
    }
    if (i == 179) {
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      unsigned char *pixels = malloc(1920 * 1080 * 4);
      assert(s && pixels);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
      for (int y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pixels + (1079-y) * 1920 * 4, 1920 * 4);
      char path[1024]; snprintf(path, sizeof path, "%s/%s.png", dir, name);
      assert(IMG_SavePNG(s, path) == 0);
      SDL_FreeSurface(s); free(pixels);
    }
    SDL_GL_SwapWindow(captureWindow);
  }
  printf("[quadro] capture=%s host=Mac-native frames=120 FPS=%.1f des=%.2fms tela=busca\n",
         name, 120000.0 / elapsed, draw / 120);
  printf("[quadro-des] capture=%s rects=%.1f fill=%.2fx cheias=%.1f tela=busca\n",
         name, rects / 120.0, fill / 120, full / 120.0);
}

static int busca_teclado_nativo(void) {
  CatItem item = {0};
  CatFileira row = {0};
  strcpy(item.imdb, "tt0133093"); strcpy(item.titulo, "Matrix");
  strcpy(item.tipo, "movie");
  strcpy(item.poster, "deploy/app/art/poster/19.jpg");
  strcpy(row.chave, "native-search"); strcpy(row.titulo, "Movies"); row.n = 8;
  CatItem items[8];
  for (int i = 0; i < 8; i++) {
    items[i] = item;
    snprintf(items[i].imdb, sizeof items[i].imdb, "tt%07d", 133093+i);
    snprintf(items[i].titulo, sizeof items[i].titulo, "Matrix %d", i+1);
    char poster[1024]; snprintf(poster, sizeof poster, "deploy/app/art/poster/%d.jpg", 19+i);
    assert(realpath(poster, items[i].poster));
  }
  cat_definir_tudo(items, 8, &row, 1); cat_quadro();
  st_teste_ligar(1);
  busca_iniciar();
  assert(st_estado() == ST_DIGITANDO && busca_foco_campo() == 1);
  st_teste_evento("Tmatrix");
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  assert(!strcmp(busca_consulta(), "matrix"));
  captureNative("typing");
  st_teste_evento("Dmatrix");
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  int bad = st_estado() != ST_PARADO || painel != 1 || !buUmaColuna() ||
            BU_RES_Y > buTopoEsq();
  printf("%s: native submit hides keyboard and results use full page\n", bad ? "FAIL" : "PASS");
  captureNative("results-full-page");
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_AC_BACK;
  busca_evento(&e);
  int backBad = busca_quer_sair() || busca_foco_campo() != 1 || st_estado() != ST_DIGITANDO;
  printf("%s: Back restores focused search home and keyboard\n", backBad ? "FAIL" : "PASS");
  captureNative("back-search-home");
  // Submit before a network response, including an empty result set.
  st_fechar(ST_BUSCA); st_ime_abrir(ST_BUSCA, "", BU_MAX_CONSULTA - 1);
  st_teste_evento("Dunmatchedquery");
  busca_atualizar(1.0f / 60.0f, SDL_GetTicks());
  int emptyBad = painel != 1 || BU_RES_Y > buTopoEsq();
  printf("%s: delayed/empty native results keep full page\n", emptyBad ? "FAIL" : "PASS");
  busca_evento(&e);
  busca_evento(&e);
  assert(busca_quer_sair());
  busca_encerrar(); st_teste_ligar(0);
  return bad | backBad | emptyBad;
}

int main(void) {
  int bad = busca_teclado_gap();
  bad |= busca_teclado_nativo();
  unsigned content = cat_revisao_conteudo(), all = cat_revisao_itens();
  CatItem extra = {0}; strcpy(extra.imdb, "tt9999999");
  assert(cat_acrescentar_lote(&extra, 1, NULL) == 1);
  assert(cat_revisao_itens() != all && cat_revisao_conteudo() == content);
  puts("PASS: Search append notifies readers without invalidating built Home rows");
  extra.naLista = 1;
  assert(cat_acrescentar_lote(&extra, 1, NULL) == 1);
  assert(cat_revisao_conteudo() != content);
  puts("PASS: Account imports still invalidate Home Library contents");
#ifndef SO_GAP
  bad |= busca_teclado_simbolos();
#endif
  return bad;
}
