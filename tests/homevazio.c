#include "homevazio.h"
#include "sync.h"
#include "qr.h"
#include <assert.h>
#include <stdio.h>
static int n, catalogo, logada, montando, pedidos;
static unsigned ultimo;
static SyncEstado estado;
int addons_n(void) { return n; }
int addons_tem_catalogo(int i) { (void)i; return catalogo; }
int sessao_logada(void) { return logada; }
int desc_montando(void) { return montando; }
SyncEstado sync_estado(void) { return estado; }
unsigned sync_ultimo_ok(void) { return ultimo; }
int sync_manual_addons(void) { pedidos++; return 1; }
int main(void) {
  SDL_Event e = {0}; Qr q;
  assert(qr_gerar(&q, HOMEVAZIO_URL));
  assert(homevazio_ativo(0));
  logada = 1; estado = SYNC_RODANDO;
  assert(!homevazio_ativo(0));
  estado = SYNC_PRONTO;
  assert(homevazio_ativo(0));
  ultimo = 1; estado = SYNC_RODANDO;
  assert(homevazio_ativo(0));
  n = 1;
  assert(homevazio_ativo(0)); // stream-only addon
  assert(!homevazio_ativo(1)); // existing rows remain available
  montando = 1; assert(!homevazio_ativo(0));
  montando = 0; catalogo = 1; assert(!homevazio_ativo(0));
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  assert(homevazio_evento(&e) && pedidos == 1);
  e.key.repeat = 1; assert(homevazio_evento(&e) && pedidos == 1);
  e.type = SDL_KEYUP; assert(homevazio_evento(&e) && pedidos == 1);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_LEFT;
  assert(!homevazio_evento(&e));
  puts("homevazio: state, sync action and QR capacity passed");
  return 0;
}
