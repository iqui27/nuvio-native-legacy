// Drive the real player handler and app sheet dispatch without a decoder.
#include "video.h"
static unsigned buffering;
static unsigned testBuffering(void) { return buffering; }
#define video_bufferando_ms testBuffering
#include "../src/player.c"
#undef video_bufferando_ms
#include "app.h"
#include <assert.h>
#include "player_back_native.h"

static void reset(void) {
  SDL_Event release = {0};
  release.type = SDL_KEYUP; release.key.keysym.sym = SDLK_ESCAPE;
  app_evento(&release);
  aberto = visivel = 1; anim = 1.0f;
  saindo = pediuSair = esperandoFonte = erroFonte = comVideo = 0;
  canalSessao = skipFoco = barraFoco = scrubbing = 0;
  buffering = 0;
  pausao_fechar(); posplay_fechar(); reacao_fechar(); intro_desligar();
}

int main(int argc, char **argv) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  assert(ajustes_shot_valor("pauseOverlayEnabled", 0));
  if (argc > 1) {
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
    reset();
    if (!strcmp(argv[1], "webos")) {
      e.key.keysym.sym = 0;
      e.key.keysym.scancode = (SDL_Scancode)482;
      dispatchNative(e);
      assert(!visivel && !saindo);
      SDL_Delay(500);
      e.key.repeat = 1; dispatchNative(e);
      assert(!visivel && !saindo && !pediuSair);
      e.key.repeat = 0;
      e.type = SDL_KEYUP; dispatchNative(e);
      e.type = SDL_KEYDOWN; dispatchNative(e);
      assert(saindo && pediuSair);
    } else if (!strcmp(argv[1], "sheets")) {
      for (int loading = 0; loading < 3; ++loading) {
        reset();
        esperandoFonte = loading == 1;
        buffering = loading == 2 ? 2000 : 0;
        faixas_abrir(); assert(faixas_aberta());
        e.key.repeat = 0; app_evento(&e);
        assert(!faixas_aberta() && visivel && !saindo && !pediuSair);
        for (int repeat = 0; repeat < 4; ++repeat) {
          e.key.repeat = repeat % 2; app_evento(&e);
          assert(visivel && !saindo && !pediuSair);
        }
        e.type = SDL_KEYUP; app_evento(&e);
        e.type = SDL_KEYDOWN; e.key.repeat = 0; app_evento(&e);
        if (loading) assert(saindo && pediuSair);
        else assert(!visivel && !saindo && !pediuSair);
      }
    } else if (!strcmp(argv[1], "timeout")) {
      app_evento(&e);
      assert(!visivel && !saindo && !pediuSair);
      e.key.repeat = 1;
      // Initial repeat delay and delayed delivery must never become new presses.
      int delays[] = {500, 1600, 150, 150};
      for (unsigned i = 0; i < sizeof delays / sizeof delays[0]; ++i) {
        SDL_Delay(delays[i]);
        app_evento(&e);
        assert(!visivel && !saindo && !pediuSair);
      }
      e.type = SDL_KEYUP; app_evento(&e);
      e.type = SDL_KEYDOWN; e.key.repeat = 0; app_evento(&e);
      assert(saindo && pediuSair);

      reset(); visivel = 0; anim = 0;
      e.key.repeat = 1; app_evento(&e);
      assert(!saindo && !pediuSair); // Even an orphan repeat is still held.

      reset(); e.key.repeat = 0;
      app_evento(&e);
      SDL_Delay(500);
      app_evento(&e);
      assert(!visivel && !saindo && !pediuSair);
      SDL_Delay(1510); // Explicit recovery for a lost keyup, non-repeat only.
      app_evento(&e);
      assert(saindo && pediuSair);
    } else if (!strcmp(argv[1], "tizen")) {
      int type, repeat;
      faixas_abrir();
      while (scanf("%d %d", &type, &repeat) == 2) {
        e.type = type ? SDL_KEYUP : SDL_KEYDOWN;
        e.key.repeat = repeat;
        app_evento(&e);
        assert(!faixas_aberta() && visivel && !saindo && !pediuSair);
      }
      e.type = SDL_KEYDOWN; e.key.repeat = 0; app_evento(&e);
      assert(!visivel && !saindo && !pediuSair);
    } else assert(!"unknown case");
    printf("PASS: %s Back regression\n", argv[1]);
    SDL_Quit();
    return 0;
  }
  SDL_Keycode keys[] = {SDLK_AC_BACK, SDLK_ESCAPE, SDLK_BACKSPACE, SDLK_DELETE};
  for (unsigned i = 0; i < sizeof keys / sizeof keys[0]; ++i) {
    SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = keys[i];
    reset();
    app_evento(&e);
    assert(!visivel && !saindo && !pediuSair);
    e.key.repeat = 1;
    for (int repeat = 0; repeat < 3; ++repeat) app_evento(&e);
    assert(!visivel && !saindo && !pediuSair);
    e.key.repeat = 0;
    app_evento(&e); // Some TV remotes repeat without SDL's repeat flag.
    assert(!visivel && !saindo && !pediuSair);
    e.type = SDL_KEYUP; app_evento(&e);
    e.type = SDL_KEYDOWN; app_evento(&e);
    assert(saindo && pediuSair);

    reset(); visivel = 0; anim = .4f;
    app_evento(&e);
    assert(!visivel && !saindo && !pediuSair);

    reset(); visivel = 0; anim = 0;
    app_evento(&e);
    assert(saindo && pediuSair);

    reset(); esperandoFonte = 1;
    app_evento(&e);
    assert(saindo && pediuSair);

    reset(); buffering = 2000;
    app_evento(&e);
    assert(saindo && pediuSair);

    reset();
    pausao_atualizar(.1f, 1, 1, -1, "", 0, 0, "", "");
    pausao_atualizar(.1f, 10001, 1, -1, "", 0, 0, "", "");
    assert(pausao_visivel());
    app_evento(&e);
    assert(!pausao_visivel() && visivel && !saindo && !pediuSair);

    reset(); posplay_shot(-1, 1, 1, 2, 0);
    assert(posplay_visivel());
    if (keys[i] != SDLK_DELETE) {
      app_evento(&e);
      assert(!posplay_visivel() && visivel && !saindo && !pediuSair);
    }

    // App dispatch owns audio, subtitles, speed and sync in the same sheet.
    if (keys[i] != SDLK_DELETE) {
      reset(); faixas_abrir(); assert(faixas_aberta());
      app_evento(&e);
      assert(!faixas_aberta() && visivel && !saindo && !pediuSair);
      reset(); esperandoFonte = 1; faixas_abrir();
      app_evento(&e);
      assert(!faixas_aberta() && !saindo && !pediuSair);
    }
  }
  reset(); skipFoco = 1;
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
  app_evento(&e);
  assert(!skipFoco && visivel && !saindo);
  reset(); visivel = 0; anim = 0;
  IntroTrecho intro = {10, 100, INTRO_ABERTURA};
  intro_shot_definir(&intro, 1); intro_definir_duracao(1800, 0);
  assert(intro_botao(20, monoSeg(), 0, 0, NULL, NULL));
  app_evento(&e);
  assert(!trechoPulavel(NULL) && !saindo);
  assert(!intro_botao(20, monoSeg(), 0, 0, NULL, NULL));
  e.type = SDL_KEYUP; app_evento(&e);
  e.type = SDL_KEYDOWN; app_evento(&e);
  assert(saindo && pediuSair);
  SDL_Quit();
  puts("PASS: Back dismisses one layer; held Back stays; release/new press exits; pure video/loading exit");
  return 0;
}
