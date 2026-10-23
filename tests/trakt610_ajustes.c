// #610: real Settings actions and native before/after screenshots, offline.
#include "../src/ajustes.c"
#define main shot_main
#include "ajustes_shot.c"
#undef main
#include "traktauth.h"
#include "trakt.h"
#include "simklauth.h"

int main(int argc, char **argv) {
  const char *out = argc > 1 ? argv[1] : "/Volumes/ExternalSSD/tmp/rev/trakt610/after.png";
  char dir[] = "/Volumes/ExternalSSD/tmp/rev/trakt610/settings-XXXXXX";
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar("deploy/app/art");
  assert(dados_gravar("trakt-p1.txt", "P1\tclient\t\t0\t0\t0"));
  assert(dados_gravar("simkl-p1.txt", "S1"));
  assert(dados_gravar("trakt-p2.txt", "P2\tclient\t\t0\t0\t0"));
  assert(dados_gravar("simkl-p2.txt", "S2"));
  traktauth_carregar_perfil(1); simklauth_carregar_perfil(1);
  assert(trakt_definir("P1", "client"));
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *win = SDL_CreateWindow("#610", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(win && SDL_GL_CreateContext(win));
  glViewport(0, 0, 1920, 1080); gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar() && txt_iniciar("deploy/app", 1));
  tex_iniciar(64); gfx_icones_dir("deploy/app/art"); ajustes_recursos("deploy/app/art");
  ajustes_iniciar(); assert(ajustes_teste_quadro("contas"));
  captura(out, win);
#ifndef TRAKT610_CAPTURE_ONLY
  {
    assert(!strcmp(textoLeitura(AJ_TRAKT), i18n("Sair da conta")));
    assert(!strcmp(textoLeitura(AJ_SIMKL), i18n("Sair da conta")));
    tecla(SDLK_RETURN);
    assert(!trakt_ativo() && traktauth_estado() == TRA_PARADO);
    assert(traktauth_desconectado());
    traktauth_carregar_perfil(1); assert(traktauth_desconectado() && !trakt_ativo());
    focarOpcao(AJ_SIMKL); tecla(SDLK_RETURN);
    assert(!simklauth_token()[0] && simklauth_estado() == SMK_PARADO);
    assert(traktauth_perfil() == 1);
    assert(traktauth_carregar_perfil(2) && simklauth_carregar_perfil(2));
    assert(traktauth_estado() == TRA_LIGADO && !strcmp(simklauth_token(), "S2"));
    puts("#610 Settings: PASS (disconnect, persistence, other profiles preserved)");
  }
#endif
  SDL_Quit();
  return 0;
}
