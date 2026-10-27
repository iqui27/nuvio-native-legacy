// O SPOTLIGHT COM O TECLADO DA TV (entrada_texto.h, caminho SDL da LG), sem TV:
// compilado com -DNV_TEXTO_SDL_TESTE, que faz texto_sistema_disponivel() dizer
// TS_TECLADO no Mac. Prova a regra do coordenador e o que o imetv MEDIU na C9:
//   - abrir NAO chama o teclado sozinho (so no Android); OK no campo chama o
//     da TV em vez do teclado do app;
//   - o texto chega como UM SDL_TEXTINPUT com a palavra inteira (ASCII), e o
//     campo vira espelho do valor inteiro;
//   - Backspace com o teclado da TV aberto apaga no valor e NAO fecha o
//     Spotlight (na C9 ele fechou sozinho, com o campo "vazio" para ele);
//   - o teclado sumir (fim sem confirmar) devolve o foco ao campo, com o texto.
#include "spotlight.h"
#include "entrada_texto.h"
#include "sistexto.h"
#include "dados.h"
#include "ajustes.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This SDL keyboard fixture models the older webOS bridge without crash injection.
int texto_teste_webos_major(void) { return 4; }
void texto_teste_crash_outro(void) { abort(); }

// Como main.c: observar ANTES do app, depois o app.
static void entregar(SDL_Event *e) { texto_sistema_observar(e); spot_evento(e); }
static void bombear(void) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) entregar(&e);
  spot_atualizar(1.0f / 60.0f, SDL_GetTicks());
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; entregar(&e);
  e.type = SDL_KEYUP; entregar(&e);
  bombear();
}
static void textinput(const char *t) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_TEXTINPUT;
  snprintf(e.text.text, sizeof e.text.text, "%s", t);
  entregar(&e);
  bombear();
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  if (!dir || !*dir) return 2;
  assert(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  ajustes_iniciar();
  assert(texto_sistema_disponivel() & TS_TECLADO);
  assert(st_ime_disponivel() && !st_abre_sozinho());

  spot_abrir(0);
  bombear();
  assert(!texto_sistema_aberto() && spot_foco_campo() == 1);
  tecla(SDLK_RETURN);                       // OK no campo: o teclado da TV
  assert(texto_sistema_aberto() && !spot_teclado_app_aberto());
  textinput("matrix");                      // a palavra inteira, de uma vez
  assert(!strcmp(spot_consulta(), "matrix"));
  tecla(SDLK_BACKSPACE);                    // apaga no valor, nao fecha
  assert(spot_aberto() && !strcmp(spot_consulta(), "matri"));
  tecla(SDLK_BACKSPACE); tecla(SDLK_BACKSPACE); tecla(SDLK_BACKSPACE);
  tecla(SDLK_BACKSPACE); tecla(SDLK_BACKSPACE); tecla(SDLK_BACKSPACE);
  assert(spot_aberto() && !spot_consulta()[0]);   // vazio E aberto
  textinput("dark");
  assert(!strcmp(spot_consulta(), "dark"));
  // O teclado some (Voltar no teclado da TV): fim sem confirmar.
  texto_sistema_fechar();
  bombear();
  assert(spot_aberto() && spot_foco_campo() == 1 && !strcmp(spot_consulta(), "dark"));
  // Sem o teclado da TV, Backspace volta a ser o do app.
  tecla(SDLK_BACKSPACE);
  assert(!strcmp(spot_consulta(), "dar"));
  spot_fechar();
  puts("PASS: Spotlight usa o teclado da TV (valor inteiro, Backspace nao fecha).");
  return 0;
}
