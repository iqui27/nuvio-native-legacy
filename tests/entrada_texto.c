// Caminho do SDL (LG e Android) de src/entrada_texto.c, com os eventos que a
// C9 MEDIU entregar (01/10/2026): TEXTINPUT com a palavra inteira, sem KEYDOWN
// de letra, e Backspace como KEYDOWN. NV_TEXTO_SDL_TESTE liga esse caminho no Mac.
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include "../src/entrada_texto.h"

static int falhas;
#define OK(c, m) do { if (c) printf("ok - %s\n", m); else { printf("FALHOU - %s\n", m); falhas++; } } while (0)

static char valor[300];
static int nTexto, nFim;

static void enviar(const SDL_Event *e) {
  // o laco do main.c: observar primeiro, depois o app
  texto_sistema_observar(e);
}
static void quadro(void) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) {
    if (e.type == texto_sistema_evento()) {
      if (e.user.code == TS_EV_TEXTO) { nTexto++; snprintf(valor, sizeof valor, "%s", texto_sistema_valor()); }
      if (e.user.code == TS_EV_FIM) nFim++;
    }
  }
}
static void texto(const char *t) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_TEXTINPUT; snprintf(e.text.text, sizeof e.text.text, "%s", t);
  enviar(&e);
}
static SDL_Event tecla(SDL_Keycode k) {
  SDL_Event e; SDL_zero(e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  return e;
}

int main(void) {
  SDL_Event e;
  SDL_Init(SDL_INIT_EVENTS);
  OK(texto_sistema_disponivel() == TS_TECLADO, "disponivel = TS_TECLADO, sem TS_VOZ");
  e = tecla(SDLK_a);
  OK(!texto_sistema_engole(&e), "fechado nao engole nada");

  texto_sistema_abrir("ma", 1);
  OK(texto_sistema_aberto(), "abre (pedir voz sem ditado abre so o teclado)");
  texto("trix");
  quadro();
  OK(!strcmp(valor, "matrix") && nTexto == 1, "TEXTINPUT soma ao texto atual");
  e = tecla(SDLK_BACKSPACE); enviar(&e);
  quadro();
  OK(!strcmp(valor, "matri"), "Backspace apaga um caractere");
  texto("x ç");
  e = tecla(SDLK_BACKSPACE); enviar(&e);
  quadro();
  OK(!strcmp(valor, "matrix "), "Backspace apaga o caractere UTF-8 inteiro");

  e = tecla(SDLK_a);       OK(texto_sistema_engole(&e), "engole letra (o IME ja a pos no valor)");
  e = tecla(SDLK_BACKSPACE); OK(texto_sistema_engole(&e), "engole Backspace (nao fecha o Spotlight vazio)");
  e = tecla(SDLK_DOWN);    OK(!texto_sistema_engole(&e), "nao engole seta");
  e = tecla(SDLK_RETURN);  OK(!texto_sistema_engole(&e), "nao engole OK");
  e = tecla(SDLK_AC_BACK); OK(!texto_sistema_engole(&e), "nao engole Voltar");
  SDL_zero(e); e.type = SDL_TEXTINPUT; OK(texto_sistema_engole(&e), "engole TEXTINPUT");

  { char longo[400]; int i; texto_sistema_fechar(); quadro(); nFim = 0;
    for (i = 0; i < 399; i++) longo[i] = 'a'; longo[399] = 0;
    texto_sistema_abrir(longo, 0);
    OK(!strcmp(texto_sistema_valor(), longo), "current text retains input beyond 255 bytes"); }
  texto_sistema_fechar();
  quadro();
  OK(nFim == 1 && !texto_sistema_aberto(), "fechar avisa o fim uma vez");
  texto_sistema_fechar();
  quadro();
  OK(nFim == 1, "fechar de novo nao repete o aviso");

  texto_sistema_ponte_valor("ponte");
  texto_sistema_abrir("", 0);
  quadro();
  OK(nTexto == 3 || strcmp(valor, "ponte"), "abrir descarta pendencia velha da ponte");
  texto_sistema_ponte_valor("de fora");
  texto_sistema_ponte_fim(1);
  quadro();
  OK(!strcmp(valor, "de fora") && nFim == 2 && !texto_sistema_aberto(), "ponte (host/JS) entrega valor e fim");
  SDL_Quit();
  return falhas ? 1 : 0;
}
