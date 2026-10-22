#include "perfilinfantil.h"
#include "perfis.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARQ "perfil-infantil.txt"   // "1,3\n": profile_index marcados
#define MAXP 32

static int marcado[MAXP + 1];
static int lido;

static void ler(void) {
  char *t;
  const char *p;
  if (lido) return;
  lido = 1;
  memset(marcado, 0, sizeof marcado);
  t = dados_ler(ARQ);
  if (!t) return;
  for (p = t; *p;) {
    char *fim;
    long v = strtol(p, &fim, 10);
    if (fim == p) { p++; continue; }
    if (v >= 1 && v <= MAXP) marcado[v] = 1;
    p = fim;
  }
  free(t);
}

static int gravar(void) {
  char b[MAXP * 4 + 4];
  int i, k = 0;
  b[0] = 0;
  for (i = 1; i <= MAXP; i++)
    if (marcado[i]) k += snprintf(b + k, sizeof b - (size_t)k, "%s%d", k ? "," : "", i);
  snprintf(b + k, sizeof b - (size_t)k, "\n");
  return dados_gravar(ARQ, b);
}

int perfil_infantil(int idx) {
  ler();
  return idx >= 1 && idx <= MAXP && marcado[idx];
}

int perfil_infantil_ativo(void) { return perfil_infantil(perfis_ativo()); }

int perfil_infantil_definir(int idx, int ligado) {
  int antes;
  ler();
  if (idx < 1 || idx > MAXP) return 0;
  ligado = ligado ? 1 : 0;
  antes = marcado[idx];
  if (antes == ligado) return 1;
  marcado[idx] = ligado;
  if (!gravar()) { marcado[idx] = antes; return 0; }
  return 1;
}

void perfil_infantil_esquecer(void) {
  memset(marcado, 0, sizeof marcado);
  lido = 1;
  dados_apagar(ARQ);
}
