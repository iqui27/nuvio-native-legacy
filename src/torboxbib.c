#include "torboxbib.h"
#include "debrid.h"
#include "js.h"
#include <stdio.h>
#include <string.h>

int torboxbib_disponivel(void) { return debrid_tb_disponivel() > 0; }

int torboxbib_carregar_pagina(int pagina, char *buf, size_t n) {
  int st = 0;
  char *json;
  if (!buf || n < 2) return 0;
  buf[0] = 0;
  if (!torboxbib_disponivel()) return 0;
  json = debrid_tb_mylist(0, 0, pagina, 0, &st);
  if (!json || st < 200 || st >= 300) { free(json); return 0; }
  snprintf(buf, n, "%s", json);
  free(json);
  return 1;
}
