#include "nvbackend.h"
#include "rede.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char *rede_baixar_st(const char *url, int segundos, const char *const *cabecalhos, int *codigo) {
  (void)url; (void)segundos; (void)cabecalhos;
  if (codigo) *codigo = 0;
  return NULL;
}

int main(void) {
  assert(nvbackend_definir_url("https://api.example.test") == 1);
  assert(!strcmp(nv_rec_url(), "https://api.example.test"));
  assert(nvbackend_definir_url("") == 1);
  puts("nvbackend: ok");
  return 0;
}
