// Real Settings row/editor and Library, with isolated synthetic credentials.
#include "../src/biblioteca.h"
#include "../src/extras.h"
#include "../src/gfx.h"
#include "../src/salvospainel.h"
#include <SDL2/SDL_image.h>
void bibfonte_library_shot(void);
void bibfonte_detail_shot(void);
static void shotFrame(int settings) {
  txt_novo_quadro(); tex_novo_quadro(); tex_bombear(4);
  glClearColor(.025f, .025f, .03f, 1); glClear(GL_COLOR_BUFFER_BIT);
  if (settings == 1) {
    ajustes_atualizar(1.0f / 60, SDL_GetTicks()); ajustes_desenhar(SDL_GetTicks());
  } else if (settings == 0) bibfonte_library_shot();
  else if (settings == 3) {
    spainel_atualizar(1.0f / 60, SDL_GetTicks()); spainel_desenhar(SDL_GetTicks());
  } else bibfonte_detail_shot();
}
static void shotSave(SDL_Window *win, const char *base, const char *name, int settings) {
  char path[1200];
  for (int i = 0; i < 80; i++) { shotFrame(settings); SDL_GL_SwapWindow(win); SDL_PumpEvents(); }
  shotFrame(settings);
  unsigned char *pixels = malloc(1920 * 1080 * 4); assert(pixels);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32); assert(s);
  for (int y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pixels + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(path, sizeof path, "%s-%s.png", base, name);
  assert(IMG_SavePNG(s, path) == 0); printf("PNG: %s\n", path);
  SDL_FreeSurface(s); free(pixels);
}
static void screenshots(const char *base) {
  assert(SDL_InitSubSystem(SDL_INIT_VIDEO) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("Library source regression", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN); assert(win);
  SDL_GLContext gl = SDL_GL_CreateContext(win); assert(gl); SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar()); assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art"); extras_carregar("deploy/app/art");
  valor[AJ_IDIOMA] = IDIOMA_EN + 1; ajustes_iniciar();
  trakt_definir("test-token", "test-client");
  if (strstr(base, "duplicate")) {
    CatItem copies[2] = {*cat_item(0), *cat_item(0)};
    copies[0].naLista = copies[1].naLista = 1;
#ifndef BIBFONTE_BASELINE
    copies[0].listaFontes = 1u << AJ_SALVOS_TRAKT;
    copies[1].listaFontes = (1u << AJ_SALVOS_TRAKT) | (1u << AJ_SALVOS_SIMKL);
#endif
    cat_definir_tudo(copies, 2, NULL, 0);
    ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
    shotSave(win, base, "detail-simkl", 2);
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
    return;
  }
  if (strstr(base, "settings-review")) {
    valor[AJ_SALVOS_DEST] = AJ_SALVOS_TRAKT;
#ifndef BIBFONTE_BASELINE
    bibliotecaFonteEscolhida = 0;
#endif
    assert(contalib_ler_biblioteca("[{\"content_id\":\"tt7654321\",\"content_type\":\"series\",\"name\":\"Account series\"}]") == 1);
    focarOpcao(AJ_SALVOS_DEST);
    shotSave(win, base, "legacy-source", 1);
    trakt_esquecer();
    assert(definirValorDireto(AJ_SALVOS_DEST, AJ_SALVOS_LOCAL));
    uxAbrirEditor(AJ_SALVOS_DEST);
    uxRestaurar = 1;
    shotSave(win, base, "restore-default", 1);
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
    return;
  }
  if (strstr(base, "sidebar")) {
    CatItem local = *cat_item(0);
    ajustes_definir_salvos_destino(AJ_SALVOS_LOCAL);
    salvos_definir(&local, 1);
    ajustes_definir_salvos_destino(AJ_SALVOS_TRAKT);
    spainel_abrir(); spainel_ir_aba(0);
    shotSave(win, base, "local-only-trakt", 3);
    SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
    return;
  }
  const char *names[] = {"nuvio", "trakt", "simkl"};
  for (int i = 0; i < 3; i++) {
    char name[80];
    focarOpcao(AJ_SALVOS_DEST); uxAbrirEditor(AJ_SALVOS_DEST);
    // Select using the same confirmation path as the Settings editor.
    uxPendente = i; uxAplicar(); assert(!uxEditor);
    snprintf(name, sizeof name, "settings-%s", names[i]); shotSave(win, base, name, 1);
    const CatItem *ci = cat_item(0);
    ilhasalvar_executar(ci, 1);
    bibfonte_library_count();
    snprintf(name, sizeof name, "library-%s", names[i]); shotSave(win, base, name, 0);
  }
  CatItem remote = *cat_item(0); remote.naLista = 1;
#ifndef BIBFONTE_BASELINE
  remote.listaFontes = 1u << AJ_SALVOS_TRAKT;
#endif
  cat_definir_tudo(&remote, 1, NULL, 0);
  ajustes_definir_salvos_destino(AJ_SALVOS_SIMKL);
  shotSave(win, base, "detail-trakt-only-simkl", 2);
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(win); SDL_Quit();
}
