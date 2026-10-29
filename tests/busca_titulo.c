// Real search selection and detail Back behavior; no window or network.
#include <assert.h>
#include "../src/busca.c"
#include "detail.h"

static void back(void) {
  SDL_Event e = {0};
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE;
  detail_evento(&e);
}

int main(void) {
  CatItem titles[2] = {0};
  HomeItem selected = {0};
  int requested;
  snprintf(titles[0].titulo, sizeof titles[0].titulo, "X");
  snprintf(titles[1].titulo, sizeof titles[1].titulo, "Y");
  for (int i = 0; i < 2; i++) {
    strcpy(titles[i].tipo, "movie");
    strcpy(titles[i].poster, "local-poster");
  }
  cat_definir_tudo(titles, 2, NULL, 0);
  selected.indice = 1;
  detail_abrir(&selected); // Previously loaded Y.
  detail_fechar_seco(); detail_atualizar(1.0f, 1000);
  assert(!detail_aberto());

  // Y was drawn; Right/Left + OK or a pointer can select X before redraw.
  painel = 1; nFil = 1; fil[0].n = 2; fil[0].pessoas = 0;
  fil[0].itens[0] = 0; fil[0].itens[1] = 1;
  focus_iniciar(&focoRes, 1, (int[]){2});
  itemFoco.indice = 1; itemFoco.arte = cat_item(1)->poster;
  itemFoco.titulo = cat_item(1)->titulo; temItemFoco = 1;
  abrirResultado();
  assert(busca_pediu_abrir(&requested) && requested == 0);
  assert(!busca_pediu_abrir(NULL));
  assert(busca_item_focado(&selected));
  assert(selected.indice == requested && !strcmp(selected.titulo, "X"));
  detail_abrir(&selected);
  assert(detail_indice() == 0 && !detail_pediu_reproduzir());

  // Internal navigation adds exactly one Back entry.
  detail_volta_notar(1); selected.indice = 1; detail_abrir(&selected);
  back(); requested = detail_pediu_abrir(); assert(requested == 0);
  detail_volta_notar(requested); selected.indice = requested; detail_abrir(&selected);
  back(); assert(detail_pediu_abrir() == -1);
  detail_atualizar(1.0f, 2000); assert(!detail_aberto());

  // A fresh search selection never inherits that internal history.
  assert(busca_item_focado(&selected)); detail_abrir(&selected);
  back(); assert(detail_pediu_abrir() == -1);
  detail_atualizar(1.0f, 3000); assert(!detail_aberto());
  puts("PASS: loaded Y, selected X before redraw opens X; Back depth 1 then 0");
  return 0;
}
