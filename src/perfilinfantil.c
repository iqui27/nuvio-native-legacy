#include "perfilinfantil.h"
#include "perfis.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char ARQ[] = "perfil-infantil.txt"; // storage filename
//    // "1,3\n": profile_index marcados
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

// --- 2.1: ajustes travados (mesmo formato de lista "1,3\n") ----------------------
static const char ARQ_TRAVA[] = "perfil-ajustes-travados.txt"; // storage filename
static int travado[MAXP + 1];
static int travadoLido;
static void lerTrava(void) {
  char *t;
  const char *p;
  if (travadoLido) return;
  travadoLido = 1;
  memset(travado, 0, sizeof travado);
  t = dados_ler(ARQ_TRAVA);
  if (!t) return;
  for (p = t; *p;) {
    char *fim;
    long v = strtol(p, &fim, 10);
    if (fim == p) { p++; continue; }
    if (v >= 1 && v <= MAXP) travado[v] = 1;
    p = fim;
  }
  free(t);
}
int perfil_ajustes_travados(int idx) {
  lerTrava();
  return idx >= 1 && idx <= MAXP && travado[idx] && perfil_infantil(idx);
}
int perfil_ajustes_travar(int idx, int ligado) {
  char b[MAXP * 4 + 4];
  int i, k = 0, antes;
  lerTrava();
  if (idx < 1 || idx > MAXP) return 0;
  ligado = ligado ? 1 : 0;
  antes = travado[idx];
  if (antes == ligado) return 1;
  travado[idx] = ligado;
  b[0] = 0;
  for (i = 1; i <= MAXP; i++)
    if (travado[i]) k += snprintf(b + k, sizeof b - (size_t)k, "%s%d", k ? "," : "", i);
  snprintf(b + k, sizeof b - (size_t)k, "\n");
  if (!dados_gravar(ARQ_TRAVA, b)) { travado[idx] = antes; return 0; }
  return 1;
}

void perfil_infantil_esquecer(void) {
  memset(marcado, 0, sizeof marcado);
  lido = 1;
  dados_apagar(ARQ);
  memset(travado, 0, sizeof travado);
  travadoLido = 1;
  dados_apagar(ARQ_TRAVA);
}
