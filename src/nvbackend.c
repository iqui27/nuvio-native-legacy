#include "nvbackend.h"
#include "dados.h"
#include "js.h"
#include "rede.h"
#include <stdio.h>
#include <string.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

static char url[400];
static int urlLida;

static void carregar(void) {
  char *t = dados_ler("nuvio-backend.txt");
  url[0] = 0;
  if (t) {
    size_t i = 0;
    while (t[i] == ' \t\n\r') i++;
    snprintf(url, sizeof url, "%s", t + i);
    url[strcspn(url, " \t\n\r")] = 0;
    free(t);
  }
}

const char *nv_rec_url(void) {
  if (!urlLida) { carregar(); urlLida = 1; }
  if (url[0]) return url;
  return NV_REC_URL;
}

static int normalizar(char *u, size_t n, const char *in) {
  size_t i = 0;
  if (!in || !in[0] || n < 8) return 0;
  while (in[i] == ' \t') i++;
  snprintf(u, n, "%s", in + i);
  u[strcspn(u, " \t\n\r")] = 0;
  if (strncmp(u, "http://", 7) && strncmp(u, "https://", 8)) return 0;
  while (u[0] && u[strlen(u) - 1] == '/') u[strlen(u) - 1] = 0;
  return u[0] != 0;
}

int nvbackend_definir_url(const char *u) {
  char nova[sizeof url] = "";
  if (u && u[0] && !normalizar(nova, sizeof nova, u)) return 0;
  snprintf(url, sizeof url, "%s", nova);
  urlLida = 1;
  dados_gravar("nuvio-backend.txt", nova);
  return 1;
}

int nvbackend_descobrir(const char *origin) {
  char base[280], pedido[320], *corpo;
  char found[sizeof url];
  int st = 0;
  if (!origin || !origin[0]) return 0;
  if (!normalizar(base, sizeof base, origin)) return 0;
  snprintf(pedido, sizeof pedido, "%s/.well-known/nuvio", base);
  corpo = rede_baixar_st(pedido, 12, NULL, &st);
  if (!corpo || st < 200 || st >= 300) { free(corpo); return 0; }
  found[0] = 0;
  if (!js_texto_raiz(corpo, "backend_url", found, sizeof found) || !found[0])
    js_texto_raiz(corpo, "url", found, sizeof found);
  free(corpo);
  if (!found[0]) return 0;
  return nvbackend_definir_url(found);
}
