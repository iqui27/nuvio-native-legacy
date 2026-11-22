#include "parear.h"
#include "js.h"
#include <stdio.h>
#include <string.h>

#define PAREAR_URL_MAX 400

int parear_codigo_valido(const char *s) {
  int i;
  if (!s) return 0;
  for (i = 0; i < PAREAR_DIGITOS; i++)
    if (s[i] < '0' || s[i] > '9') return 0;
  return s[PAREAR_DIGITOS] == 0;
}

void parear_formatar(const char *codigo, char *dst, size_t tam) {
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!parear_codigo_valido(codigo)) return;
  snprintf(dst, tam, "%.3s %.3s", codigo, codigo + 3);
}

static int httpOk(const char *url) {
  const char *p;
  size_t n;
  if (!url) return 0;
  if (!strncmp(url, "https://", 8)) p = url + 8;
  else if (!strncmp(url, "http://", 7)) p = url + 7;
  else return 0;
  n = strlen(url);
  if (!*p || *p == '/' || n > PAREAR_URL_MAX) return 0;
  for (; *p; p++)
    if ((unsigned char)*p <= ' ' || *p == '"' || *p == '\'' || *p == '<' || *p == '>' || *p == '\\')
      return 0;
  return 1;
}

int parear_endereco_ok(const char *url) { return httpOk(url); }

int parear_ler_resposta(int status, const char *corpo, char *codigo, size_t nCodigo,
                        char *pagina, size_t nPagina, int *validade) {
  char c[16], p[200];
  const char *fim;
  int v;
  if (status != 200 || !corpo || !codigo || !pagina || nCodigo <= PAREAR_DIGITOS || !nPagina) return 0;
  fim = corpo + strlen(corpo);
  if (!js_texto(corpo, fim, "codigo", c, sizeof c) || !parear_codigo_valido(c)) return 0;
  if (!js_texto(corpo, fim, "pagina", p, sizeof p) || !httpOk(p) || strlen(p) >= nPagina) return 0;
  v = (int)js_num(corpo, fim, "validade", 0.0);
  if (v < 1 || v > 3600) return 0;
  snprintf(codigo, nCodigo, "%s", c);
  snprintf(pagina, nPagina, "%s", p);
  if (validade) *validade = v;
  return 1;
}

void parear_pagina_curta(const char *pagina, char *dst, size_t tam) {
  size_t n;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!pagina) return;
  if (!strncmp(pagina, "https://", 8)) pagina += 8;
  else if (!strncmp(pagina, "http://", 7)) pagina += 7;
  if (!strncmp(pagina, "www.", 4)) pagina += 4;
  snprintf(dst, tam, "%s", pagina);
  n = strlen(dst);
  while (n > 0 && dst[n - 1] == '/') dst[--n] = 0;
}

int parear_espera(int status, int tentativa) {
  if (tentativa >= 3) return -1;
  if (status == 200) return 5;                 // 200 com corpo que nao serve
  if (status == 429) return 60;
  if (status >= 400 && status < 500) return -1;
  if (status == 503) return -1;
  return tentativa <= 1 ? 5 : 20;              // sem resposta ou 5xx passageiro
}

int parear_vencendo(long agora, long expira) { return expira - agora < 30; }
