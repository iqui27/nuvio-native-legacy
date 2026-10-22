// A PRESSAO LONGA NO CARTAZ, do limiar ate o modal nao clicar sozinho.
//
// Relato do dono: "o botao de segurar os cards ta muito rapido e acaba
// clicando duas vezes quando seguro ele... ja clica automatico quando abre o
// modal e tiver segurando".
//
// Sao dois defeitos no mesmo gesto:
//   - a home abria o menu em NV_HOLD_FEEDBACK_MS (110 ms) em vez de
//     NV_HOLD_MS, ou seja em menos que um toque comum;
//   - o modal abre COM O DEDO AINDA NO BOTAO (home.c dispara no limiar, nao no
//     KEYUP), e a repeticao automatica do controle virava escolha imediata.
#include "ctxmenu.h"
#include "catalogo.h"
#include "layout.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void tecla(int tipo, int k, int repeticao) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo;
  e.key.keysym.sym = k;
  e.key.repeat = repeticao;
  ctx_evento(&e);
}

int main(void) {
  CatItem c = { 0 };

  // O limiar e um so no app inteiro, e nao pode voltar a ser curto.
  assert(NV_HOLD_MS >= 700);

  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.titulo, sizeof c.titulo, "Sinners");
  snprintf(c.imdb, sizeof c.imdb, "tt31193180");
  // Com progresso: a ultima opcao e "Tirar de Continuar assistindo", que so
  // ABRE A CONFIRMACAO (nada e gravado). E por ela que o teste ve se um OK
  // escolheu: com a confirmacao aberta o Voltar volta ao menu; sem ela, fecha.
  c.progresso = 42; c.restanteMin = 51;
  cat_definir(&c, 1);

  // 1. O OK QUE ABRIU NAO ESCOLHE. Enquanto o dedo nao sobe, o modal fica de
  //    pe e nenhuma opcao e acionada.
  ctx_abrir(0);
  assert(ctx_aberto());
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 1);   // repeticao do controle
  assert(ctx_aberto());
  tecla(SDL_KEYDOWN, SDLK_ESCAPE, 0);   // sem confirmacao aberta, Voltar fecha
  assert(!ctx_aberto() && cat_item(0)->progresso == 42);
  puts("ok  o OK que abriu o modal nao escolhe nada");

  // 2. DEPOIS DE SOLTAR, um toque novo escolhe: a confirmacao abre, e o
  //    Voltar dela devolve ao menu (que segue aberto).
  ctx_abrir(0);
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_ESCAPE, 0);
  assert(ctx_aberto() && cat_item(0)->progresso == 42);
  tecla(SDL_KEYDOWN, SDLK_ESCAPE, 0);
  assert(!ctx_aberto());
  puts("ok  um toque novo escolhe");

  // 3. REPETICAO NUNCA E ESCOLHA, nem com o modal ja liberado.
  ctx_abrir(0);
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 1);
  tecla(SDL_KEYDOWN, SDLK_ESCAPE, 0);
  assert(!ctx_aberto());
  puts("ok  repeticao automatica nao escolhe");

  // 4. A ULTIMA OPCAO RESPONDE. "No LG ele nao seleciona, ele pula e nao faz
  //    nada" — "Tirar de Continuar assistindo" e a ultima da lista, e e ela
  //    que some quando montar() encolhe. Aqui o foco desce ate ela e o OK tem
  //    de agir: a posicao de retomada some e o modal fecha.
  ctx_abrir(0);
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);   // salvar -> assistido -> retomada (o foco para na ultima)
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  // 5. TIRAR SO DEPOIS DA CONFIRMACAO (dono, 02/10). O OK na opcao abre a
  //    pergunta e nada e apagado; Voltar e "Cancelar" devolvem ao menu sem
  //    tocar na retomada; so o OK em "Tirar da fileira" (o foco inicial) tira.
  assert(ctx_aberto() && cat_item(0)->progresso == 42);
  tecla(SDL_KEYDOWN, SDLK_ESCAPE, 0);                  // Voltar = cancelar
  assert(ctx_aberto() && cat_item(0)->progresso == 42);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);                  // de volta a pergunta
  tecla(SDL_KEYDOWN, SDLK_RIGHT, 0);                   // "Cancelar"
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(ctx_aberto() && cat_item(0)->progresso == 42);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);                  // pergunta de novo
  tecla(SDL_KEYDOWN, SDLK_RETURN, 1);                  // repeticao nao confirma
  assert(ctx_aberto() && cat_item(0)->progresso == 42);
  tecla(SDL_KEYDOWN, SDLK_RIGHT, 0);
  tecla(SDL_KEYDOWN, SDLK_LEFT, 0);                    // "Tirar da fileira"
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(!ctx_aberto());
  assert(cat_item(0)->progresso == 0);
  puts("ok  tirar de Continuar so depois do OK em \"Tirar da fileira\"");
  puts("ok  a ultima opcao do modal responde ao OK");

  puts("hold: tudo ok");
  return 0;
}
