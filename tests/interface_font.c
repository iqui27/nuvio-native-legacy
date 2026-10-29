// The real Settings preference transaction and text renderer must agree.
#include "../src/ajustes.c"
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <unistd.h>

static void importar(const char *dir, const char *font) {
  char path[1024];
  snprintf(path, sizeof path, "%s/fontes-importadas/entrada-ab.tmp", dir);
  FILE *in = fopen(font, "rb"), *out = fopen(path, "wb"); assert(in && out);
  char b[8192]; size_t n;
  while ((n = fread(b, 1, sizeof b, in))) assert(fwrite(b, 1, n, out) == n);
  assert(!ferror(in)); fclose(in); assert(!fclose(out));
  assert(legfontes_importar(LEGFONTE_INTERFACE, "entrada-ab.tmp"));
}
static void verificar(TxtEstilo estilo, const char *s) {
  int size, peso, w, h;
  txt_estilo_medidas(estilo, &size, &peso);
  TTF_Font *face = TTF_OpenFont(legfontes_caminho(LEGFONTE_INTERFACE), size); assert(face);
  if (peso == 2) TTF_SetFontStyle(face, TTF_STYLE_BOLD);
  assert(!TTF_SizeUTF8(face, s, &w, &h)); TTF_CloseFont(face);
  txt_novo_quadro();
  assert(txt_largura(estilo, s) == w);
  TxtLinha l = txt_linha(estilo, s, 255, 255, 255, 255);
  assert(l.tex && l.w == w);
}
int main(int argc, char **argv) {
  setbuf(stdout, NULL);
  assert(argc == 3); const char *dir = argv[1];
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER));
#ifdef __APPLE__
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
  SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#endif
  SDL_Window *win = SDL_CreateWindow("Imported interface font", 0, 0, 64, 64,
    SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win);
  if (!gl) fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError());
  assert(gl);
  gfx_tamanho_alvo(1920, 1080); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  player_dir(dir); ajustes_dir(dir);
  int subtitle = txt_largura_familia(TXT_LEG_120, "iiiiWWWW0123", TXT_FAMILIA_INTER);
  importar(dir, argv[2]); ajustes_fontes_atualizar();
  assert(txt_fonte_interface() == TXT_FAMILIA_IMPORTADA_UI);
  assert(legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == -1);
  assert(ajFonteAtual(AJ_FONTE_UI) == TXT_FAMILIA_N);
  assert(!strcmp(textoValor(AJ_FONTE_UI), legfontes_nome(0)));
  verificar(TXT_BODY, "iiiiWWWW0123"); verificar(TXT_TITULO1, "Settings");
  verificar(TXT_DET_SIN, "A longer title synopsis that wraps across lines");
  assert(txt_largura_familia(TXT_LEG_120, "iiiiWWWW0123", TXT_FAMILIA_INTER) == subtitle);
  // Fallback continues to use the existing language shaping/glyph logic.
  char out[1600]; int runs;
  const char *languages[] = {"Türkçe", "Ελληνικά", "日本語", "العربية"};
  for (unsigned i = 0; i < sizeof languages / sizeof *languages; i++) {
    int baseline = txt_visual_da_linha(TXT_FAMILIA_INTER, TXT_BODY,
      languages[i], out, sizeof out, &runs);
    int custom = txt_visual_da_linha(TXT_FAMILIA_IMPORTADA_UI, TXT_BODY,
      languages[i], out, sizeof out, &runs);
    printf("fallback %s: bundled=%d imported=%d\n", languages[i], baseline, custom);
    assert(custom >= 0 && custom <= baseline);
  }
  // A screen uses many styles at once. Returning to it must hit cached faces
  // and textures, rather than opening a new face for every label each frame.
  for (int frame = 0; frame < 3; frame++) {
    txt_novo_quadro();
    int before = txt_rasterizadas;
    const float scales[] = {1.0f, 1.2f, 1.5f};
    for (unsigned i = 0; i < sizeof scales / sizeof *scales; i++) {
      float old = gfx_escala(); gfx_escala_sair(scales[i]);
      assert(txt_linha(TXT_BODY, "Home", 255, 255, 255, 255).tex);
      assert(txt_linha(TXT_CAPTION, "Sources", 255, 255, 255, 255).tex);
      assert(txt_linha(TXT_TITULO1, "A title", 255, 255, 255, 255).tex);
      gfx_escala_sair(old);
    }
    if (frame) assert(txt_rasterizadas == before);
  }
  txt_novo_quadro(); TxtLinha cut = txt_linha_corta(TXT_BODY,
    "A long imported-font menu label that must fit", 255, 255, 255, 255, 180);
  assert(cut.tex && cut.w <= 180);
  // Both subtitle overrides stay independent of interface choices.
  assert(legfontes_escolher(0, 0) && legfontes_escolher(1, 0));
  char tmp[1024]; snprintf(tmp, sizeof tmp, "%s/ajustes.tmp", dir); assert(!mkdir(tmp, 0700));
  assert(!definirValorDireto(AJ_FONTE_UI, TXT_FAMILIA_MONTSERRAT));
  assert(txt_fonte_interface() == TXT_FAMILIA_IMPORTADA_UI);
  assert(legfontes_selecionada(LEGFONTE_INTERFACE) == 0);
  assert(!rmdir(tmp));
  assert(definirValorDireto(AJ_FONTE_UI, TXT_FAMILIA_MONTSERRAT));
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  assert(legfontes_selecionada(LEGFONTE_INTERFACE) == -1);
  assert(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  assert(definirValorDireto(AJ_FONTE_UI, TXT_FAMILIA_N));
  // Simulate restart: no transient imported index may be stored in ajustes.
  legfontes_dir(NULL); legfontes_dir(dir); ajustes_dir(dir);
  assert(valor[AJ_FONTE_UI] == TXT_FAMILIA_MONTSERRAT);
  assert(txt_fonte_interface() == TXT_FAMILIA_IMPORTADA_UI);
  assert(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  assert(legfontes_remover(0)); ajustes_fontes_atualizar();
  assert(txt_fonte_interface() == TXT_FAMILIA_MONTSERRAT);
  assert(legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == -1);
  txt_novo_quadro(); assert(txt_linha(TXT_BODY, "Settings", 255, 255, 255, 255).tex);
  txt_encerrar(); SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
  puts("interface_font: Settings, persistence, rollback, independent subtitles, metrics, cache and fallback passed");
  return 0;
}
