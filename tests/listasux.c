// Lists focus regression. Include the real handlers; no account or network.
#define lst_buscar listasux_buscar
#define lst_pedir listasux_pedir
#define lst_pedir_trilhas listasux_trilhas
#include "../src/biblioteca.c"
static int requested;
void listasux_buscar(const char *q) { (void)q; requested = 2; }
void listasux_pedir(int f) { (void)f; requested = 0; }
void listasux_trilhas(void) { requested = 3; }
#include <assert.h>
static void key(SDL_Keycode code) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
  biblioteca_evento_(&e);
}
int main(void) {
  modo = MODO_LISTAS; temAberta = 0; listaTab = 0;
  origensAbertas = focoOrigem = focoBotaoOrig = 0;
  foco.fileira = BIB_FIL_MODO; foco.coluna = MODO_LISTAS;
  assert(bibCompacto() && botaoOrigens() && !origensNaGrade());
  key(SDLK_LEFT); assert(foco.coluna == MODO_COLECOES);
  key(SDLK_RIGHT); assert(foco.coluna == MODO_LISTAS);
  key(SDLK_DOWN); assert(foco.fileira == BIB_FIL_PICK);
  focoBotaoOrig = 0; pickSel = foco.coluna = 0;
  key(SDLK_RIGHT); assert(pickSel == 1);
  key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(listaTab == 2 && requested == 2 && foco.fileira == BIB_FIL_PICK);
  key(SDLK_RIGHT); key(SDLK_RETURN);
  assert(listaTab == 3 && requested == 3);
  key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); assert(pickSel == 0);
  key(SDLK_LEFT); assert(focoBotaoOrig);
  key(SDLK_RETURN); assert(origensAbertas);
  key(SDLK_DOWN); assert(focoOrigem);
  key(SDLK_ESCAPE); assert(!origensAbertas && !focoOrigem && focoBotaoOrig);
  assert(modo == MODO_LISTAS);
  key(SDLK_RETURN); assert(origensAbertas);
  key(SDLK_RETURN); assert(!origensAbertas);
  focoBotaoOrig = 0; key(SDLK_UP);
  assert(foco.fileira == BIB_FIL_MODO && foco.coluna == MODO_LISTAS);
  puts("Lists navigation: PASS");
  return 0;
}
