#include "torboxbib.h"
#include "debrid.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int torboxbib_disponivel(void) { return debrid_tb_disponivel() > 0; }

char *torboxbib_carregar_json(int pagina, int *stOut) {
  int st = 0;
  char *json;
  if (stOut) *stOut = 0;
  if (!torboxbib_disponivel()) return NULL;
  json = debrid_tb_mylist(0, 0, pagina, 0, &st);
  if (stOut) *stOut = st;
  if (!json || st < 200 || st >= 300) { free(json); return NULL; }
  return json;
}

int torboxbib_carregar_pagina(int pagina, char *buf, size_t n) {
  char *json;
  size_t len;
  if (!buf || n < 2) return 0;
  buf[0] = 0;
  json = torboxbib_carregar_json(pagina, NULL);
  if (!json) return 0;
  len = strlen(json);
  if (len >= n) { free(json); return 0; }
  memcpy(buf, json, len + 1);
  free(json);
  return 1;
}

int torboxbib_contar_itens(const char *json) {
  const char *d;
  int n = 0;
  if (!json || !json[0]) return 0;
  d = strstr(json, "\"data\"");
  if (!d) return 0;
  d = strchr(d, '[');
  if (!d) return 0;
  for (d++; *d && *d != ']'; d++)
    if (*d == '{') n++;
  return n;
}
