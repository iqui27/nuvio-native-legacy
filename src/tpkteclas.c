// Ver tpkteclas.h.
#include "tpkteclas.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>

int tpkteclas_evento(const char *nome, int apertou, SDL_Event *e) {
  // Mesma traducao do .wgt (tools/tizen-shell.html), para os dois pacotes da
  // Samsung se comportarem igual. Nomes: developer.samsung.com/smarttv/develop/
  // tizen-net-tv/guides/user-interaction.html
  //  - toda tecla de play/pause vira SDLK_PAUSE (player.c alterna e poe o foco
  //    no Play; o app nao tem "so pausar"), inclusive XF86PlayBack, que e o
  //    play/pause do Smart Remote 2021+;
  //  - Stop volta; retroceder/avancar viram as setas (seek do player);
  //  - a azul vira "s" (Salvos), vermelha/verde abrem o painel de log (F9).
  static const struct { const char *n; SDL_Keycode k; } T[] = {
    { "Up", SDLK_UP }, { "Down", SDLK_DOWN }, { "Left", SDLK_LEFT }, { "Right", SDLK_RIGHT },
    { "Return", SDLK_RETURN }, { "KP_Enter", SDLK_RETURN }, { "Select", SDLK_RETURN },
    { "XF86Back", SDLK_AC_BACK }, { "Escape", SDLK_AC_BACK }, { "BackSpace", SDLK_BACKSPACE },
    { "XF86PlayBack", SDLK_PAUSE }, { "XF86AudioPlay", SDLK_PAUSE }, { "XF86AudioPause", SDLK_PAUSE },
    { "XF86AudioPlayPause", SDLK_PAUSE }, { "XF86AudioStop", SDLK_AC_BACK },
    { "XF86AudioRewind", SDLK_LEFT }, { "XF86AudioForward", SDLK_RIGHT },
    { "XF86AudioNext", SDLK_RIGHT }, { "XF86AudioPrev", SDLK_LEFT },
    { "XF86NextChapter", SDLK_RIGHT }, { "XF86PreviousChapter", SDLK_LEFT },
    // CH+/CH-: F7/F8, como o Android entrega. main.c os vira em CH+/- de
    // verdade com canal na tela (zap) e em Salvos / Spotlight fora disso. Era
    // so o CH+ -> "s", e o CH- nao existia: sem zap na Samsung (dono, 05/10).
    { "XF86RaiseChannel", SDLK_F7 }, { "XF86LowerChannel", SDLK_F8 }, { "XF86Blue", SDLK_s },
    { "XF86Red", SDLK_F9 }, { "XF86Green", SDLK_F9 },
    // GUIA (o botao entre CH+ e CH- do controle): sem reserva a TV abre o guia
    // dela e o app sai (dono, 05/10). Vira o Guia de TV do app (F10, app.c).
    // Nomes da lista da Samsung; o do Q80A ainda nao apareceu num log.
    { "XF86ChannelGuide", SDLK_F10 }, { "XF86ChannelList", SDLK_F10 },
#ifdef NV_ASPECTO_DIAG
    { "x", SDLK_x }, { "X", SDLK_x },
#endif
    { "Minus", SDLK_MINUS },
    // SPOTLIGHT (spotlight.h): o microfone do Smart Remote CHEGA como
    // XF86BTVoice (MEDIDO no D1, 1.6.0 Tizen 6+, "tpk sem mapa: XF86BTVoice").
    // F6 = abrir pela voz; amarela = F5 = so abrir. XF86Search (controles de
    // botao "Search") e suposto pelo nome, nao visto no log.
    { "XF86BTVoice", SDLK_F6 }, { "XF86Search", SDLK_F6 }, { "XF86Yellow", SDLK_F5 },
  };
  SDL_Keycode k = SDLK_UNKNOWN;
  size_t i;
  if (!nome || !e) return 0;
  for (i = 0; i < sizeof T / sizeof T[0]; i++)
    if (!strcmp(nome, T[i].n)) { k = T[i].k; break; }
  if (k == SDLK_UNKNOWN && nome[0] >= '0' && nome[0] <= '9' && !nome[1]) k = (SDL_Keycode)nome[0];
  if (k == SDLK_UNKNOWN) {
    printf("[tecla] tpk sem mapa: %s\n", nome);
    fflush(stdout);
    return 0;
  }
  SDL_zero(*e);
  e->type = apertou ? SDL_KEYDOWN : SDL_KEYUP;
  e->key.state = apertou ? SDL_PRESSED : SDL_RELEASED;
  e->key.keysym.sym = k;
  e->key.keysym.scancode = SDL_GetScancodeFromKey(k);
  // A AZUL do controle e a AZUL do app, nao a letra "s".
  if (!strcmp(nome, "XF86Blue")) e->key.keysym.scancode = (SDL_Scancode)NV_SCANCODE_BLUE;
  return 1;
}
