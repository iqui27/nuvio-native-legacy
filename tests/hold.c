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
  cat_definir(&c, 1);

  // 1. O OK QUE ABRIU NAO ESCOLHE. Enquanto o dedo nao sobe, o modal fica de
  //    pe e nenhuma opcao e acionada.
  ctx_abrir(0);
  assert(ctx_aberto());
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(ctx_aberto() && ctx_pediu_detalhes() < 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 1);   // repeticao do controle
  assert(ctx_aberto() && ctx_pediu_detalhes() < 0);
  puts("ok  o OK que abriu o modal nao escolhe nada");

  // 2. DEPOIS DE SOLTAR, um toque novo escolhe. "Ver detalhes" e a primeira
  //    opcao, e ela fecha o modal.
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(!ctx_aberto());
  assert(ctx_pediu_detalhes() == 0);
  puts("ok  um toque novo escolhe");

  // 3. REPETICAO NUNCA E ESCOLHA, nem com o modal ja liberado.
  ctx_abrir(0);
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 1);
  assert(ctx_aberto() && ctx_pediu_detalhes() < 0);
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(!ctx_aberto() && ctx_pediu_detalhes() == 0);
  puts("ok  repeticao automatica nao escolhe");

  // 4. A ULTIMA OPCAO RESPONDE. "No LG ele nao seleciona, ele pula e nao faz
  //    nada" — "Tirar de Continuar assistindo" e a ultima da lista, e e ela
  //    que some quando montar() encolhe. Aqui o foco desce ate ela e o OK tem
  //    de agir: a posicao de retomada some e o modal fecha.
  { CatItem c2 = *cat_item(0);
    c2.progresso = 42; c2.restanteMin = 51;
    cat_definir(&c2, 1); }
  ctx_abrir(0);
  tecla(SDL_KEYUP, SDLK_RETURN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);
  tecla(SDL_KEYDOWN, SDLK_DOWN, 0);   // Ver detalhes -> biblioteca -> assistido -> retomada
  tecla(SDL_KEYDOWN, SDLK_RETURN, 0);
  assert(!ctx_aberto());
  assert(cat_item(0)->progresso == 0);
  puts("ok  a ultima opcao do modal responde ao OK");

  puts("hold: tudo ok");
  return 0;
}
