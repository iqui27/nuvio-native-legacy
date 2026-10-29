#include "ajustes.h"
// Ajuste "Teclado" na LG (build com -DNV_WEBOS): a opcao aparece na lista e
// "Do app" faz a decisao do teclado devolver a grade, nunca o sistema.
#include "dados.h"
#include "sistexto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static char dir[] = "/tmp/nuvio-teclado-webos-XXXXXX", caminho[700];
static void modo(int v) {
  FILE *f = fopen(caminho, "w"); assert(f);
  fprintf(f, "tecladoAndroidLocal %d\n", v); fclose(f);
  ajustes_dir(dir);
  assert(ajustes_teclado_android() == v);
}

int main(void) {
  assert(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) == 0);
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
#ifndef NV_TECLADO_SIS
  assert(!"NV_TECLADO_SIS ausente");
#endif
  assert(ajustes_teclado_na_tela() == 1);
  modo(0); assert(st_teclado_modo() == 0 && !st_teclado_do_app());
  modo(1); assert(st_teclado_modo() == 1 && st_teclado_do_app() && !st_ime_disponivel());
  modo(2); assert(st_teclado_modo() == 2 && !st_teclado_do_app());
  puts("teclado_webos: ok");
  return 0;
}
