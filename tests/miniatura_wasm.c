// WASM always counts as little memory under the guide rule (memoria.h): the
// .wgt never offers "No aparelho". Compiled with -D__EMSCRIPTEN__ on the host
// by miniatura_tv.sh — the rest of miniatura.c is an empty TU there anyway.
#include <assert.h>
#include <stdio.h>
#include "../src/memoria.h"
int main(void) {
  assert(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 8192));
  assert(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 4096));
  assert(epg_memoria_eh_baixa(EPG_MEMORIA_AUTO, 0));
  assert(epg_memoria_eh_baixa(EPG_MEMORIA_BAIXA, 8192));
  assert(!epg_memoria_eh_baixa(EPG_MEMORIA_COMPLETA, 512));
  puts("PASS: WASM e sempre 'pouca memoria' no automatico");
  return 0;
}
