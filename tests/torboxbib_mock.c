#include "torboxbib.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *debrid_tb_mylist(int usenet, int item, int pagina, int fresco, int *stOut) {
  static const char *lista =
    "{\"success\":true,\"data\":[{\"id\":1,\"name\":\"A\",\"hash\":\"a\"},{\"id\":2,\"name\":\"B\"}]}";
  static const char *vazio = "{\"success\":true,\"data\":[]}";
  static const char *erro = "{\"success\":false}";
  if (stOut) *stOut = 200;
  if (pagina == 99) return strdup(erro);
  if (pagina == 1) return strdup(vazio);
  if (pagina == 2) {
    if (stOut) *stOut = 503;
    return NULL;
  }
  (void)usenet;
  (void)item;
  (void)fresco;
  return strdup(lista);
}

int debrid_tb_disponivel(void) { return 1; }

int main(void) {
  int st = 0;
  char *j = torboxbib_carregar_json(0, &st);
  assert(j && st == 200);
  assert(torboxbib_contar_itens(j) == 2);
  free(j);
  j = torboxbib_carregar_json(1, &st);
  assert(j && torboxbib_contar_itens(j) == 0);
  free(j);
  assert(!torboxbib_carregar_json(2, &st));
  puts("torboxbib: ok");
  return 0;
}
