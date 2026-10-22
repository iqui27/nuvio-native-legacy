#include "colfileiras.h"
#include "colecoes.h"
#include "catordem.h"
#include "fileiras.h"
#include "addons.h"
#include "sessao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int chaveFonte(const ColSource *s, char *dst) {
  if (!s || s->prov[0] || !s->type[0] || !s->catId[0]) return 0;
  const char *id = s->addonId;
  if (!id[0] && s->base[0]) {
    for (int i = 0; i < addons_n(); i++)
      if (!strcmp(addons_base(i), s->base)) { id = addons_id_manifesto(i); break; }
    if (!id || !id[0]) id = s->base;
  }
  if (!id || !id[0]) return 0;
  int n = snprintf(dst, FIL_CHAVE, "%s_%s_%s", id, s->type, s->catId);
  return n > 0 && n < FIL_CHAVE;
}

// #294: WHICH PROFILE THE ACCOUNT COLLECTIONS IN MEMORY BELONG TO. On a switch
// fileiras.c loads the new profile's rows at once, but the previous profile's
// collections stay in colecoes.c until app.c calls col_esquecer_perfil a few
// frames later. A Home pass in that window ran the authoritative reconcile with
// profile B's collections against profile A's rows: every A collection B lacks
// was deleted from A's file and B's were appended, so when A's own collections
// arrived they were re-registered at the END ("collections pushed to the
// bottom"). A snapshot from another profile is now ignored until it is replaced.
static unsigned geracaoCol;
static int colDoPerfil(void) {
  return !col_tem_conta() || geracaoCol == fil_perfil_geracao();
}

void colfileiras_contexto(void) {
  if (fil_conta_dono(sessao_usuario()) && col_tem_conta()) col_esquecer_perfil();
}

int colfileiras_receber(const char *json) {
  colfileiras_contexto();
  unsigned rev = col_revisao();
  int cap = 0, n = 0;
  // SEM VALIDAR DUAS VEZES. col_definir_json valida a resposta inteira antes de
  // mexer em qualquer coisa; validar aqui tambem dobrava o custo do ciclo de
  // sync (tests/sync_aplicar_perf.sh: a validacao era metade do tempo). Resposta
  // invalida nao muda col_revisao(), e entao `antes` simplesmente nao e usado.
  // The removal diff compares the previous snapshot with this one; a previous
  // snapshot from another profile says nothing about this profile's rows.
  int mesmoPerfil = col_tem_conta() && geracaoCol == fil_perfil_geracao();
  if (mesmoPerfil)
    for (int i = 0; i < col_n(); i++) {
      const ColFolder *f = col_folder(i);
      if (f && !f->extra) cap += f->nSources;
    }
  char (*antes)[FIL_CHAVE] = cap ? calloc((size_t)cap, FIL_CHAVE) : NULL;
  if (cap && !antes) {
    printf("[collections] account snapshot deferred: allocation failed\n");
    return 0;
  }
  if (antes) for (int i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    if (!f || f->extra) continue;
    char grupo[FIL_CHAVE]; col_chave_pasta(f, grupo, sizeof grupo);
    // A hidden group does not represent the independently selected rows.
    if (fil_oculta(grupo)) continue;
    for (int j = 0; j < f->nSources && n < cap; j++)
      if (chaveFonte(&f->sources[j], antes[n])) n++;
  }
  int r = col_definir_json(json);
  if (col_tem_conta()) geracaoCol = fil_perfil_geracao();
  if (antes && rev != col_revisao()) for (int i = 0; i < n; i++) {
    int existe = 0;
    for (int j = 0; j < col_n() && !existe; j++) {
      const ColFolder *f = col_folder(j);
      for (int k = 0; f && k < f->nSources && !existe; k++) {
        char chave[FIL_CHAVE];
        existe = chaveFonte(&f->sources[k], chave) && !strcmp(chave, antes[i]);
      }
    }
    for (int j = 0; !existe && j < catordem_n(); j++)
      existe = !strcmp(catordem_chave(j), antes[i]) && !catordem_oculta(antes[i], antes[i]);
    if (!existe) fil_colecao_catalogo_removido(antes[i]);
  }
  free(antes);
  return r;
}

void colfileiras_sincronizar(void) {
  colfileiras_contexto();
  if (!colDoPerfil()) {
    printf("[collections] row sync skipped: collections in memory belong to the previous profile\n");
    return;
  }
  // NA PILHA ERAM COL_MAX x 192 bytes (48 KB com 256 pastas; 384 KB com o teto
  // de 2048 do #255). Grupos sao poucos, mas o pior caso e um grupo por pasta.
  int max = col_n() > 0 ? col_n() : 1, n = 0;
  char (*ids)[FIL_CHAVE] = malloc((size_t)max * FIL_CHAVE);
  const char **chaves = malloc(sizeof *chaves * (size_t)max);
  const char **titulos = malloc(sizeof *titulos * (size_t)max);
  int *ocultas = malloc(sizeof *ocultas * (size_t)max);
  if (!ids || !chaves || !titulos || !ocultas) {
    printf("[collections] row sync deferred: allocation failed\n");
    free(ids); free(chaves); free(titulos); free(ocultas);
    return;
  }
  for (int i = 0; i < col_n() && n < max; i++) {
    const ColFolder *f = col_folder(i);
    if (!f || !f->group[0]) continue;
    for (int j = 0; j < f->nSources; j++) {
      char chave[FIL_CHAVE];
      if (chaveFonte(&f->sources[j], chave)) fil_colecao_catalogo_restaurado(chave);
    }
    col_chave_pasta(f, ids[n], sizeof ids[n]);
    int j;
    for (j = 0; j < n; j++) if (!strcmp(ids[j], ids[n])) break;
    if (j < n) continue;
    chaves[n] = ids[n];
    titulos[n] = f->group;
    ocultas[n] = catordem_oculta(chaves[n], chaves[n]);
    n++;
  }
  fil_colecoes_reconciliar(chaves, titulos, ocultas, n, col_tem_conta());
  free(ids); free(chaves); free(titulos); free(ocultas);
  // Use a locked registry snapshot: discovery may be registering catalogues
  // on its worker while the account pull is applied on the drawing thread.
  static char todas[FIL_MAX][FIL_CHAVE];
  const char *conhecidas[FIL_MAX], *ordenadas[FIL_MAX];
  int ordem[FIL_MAX], fora[FIL_MAX], dentro[FIL_MAX] = {0};
  int q = fil_copiar_chaves(todas, FIL_MAX);
  for (int i = 0; i < q; i++) conhecidas[i] = todas[i];
  q = catordem_unir(conhecidas, q, ordem, FIL_MAX);
  for (int i = 0; i < q; i++) {
    ordenadas[i] = conhecidas[ordem[i]];
    fora[i] = catordem_oculta(ordenadas[i], ordenadas[i]);
    if (!fora[i]) for (int j = 0; j < catordem_n(); j++)
      if (!strcmp(ordenadas[i], catordem_chave(j)))
        fil_colecao_catalogo_restaurado(ordenadas[i]);
  }
  // Restoring an automatic exclusion does not make a wrapped source a
  // standalone Home row. Keep collection membership separate from personal
  // visibility, or these invisible sources steal catalogue quota (#233).
  for (int i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    if (!f) continue;
    char grupo[FIL_CHAVE]; col_chave_pasta(f, grupo, sizeof grupo);
    if (fil_oculta(grupo) || catordem_oculta(grupo, grupo)) continue;
    for (int j = 0; j < f->nSources; j++) {
      char chave[FIL_CHAVE];
      if (chaveFonte(&f->sources[j], chave) && !fil_adicionada_na_tv(chave))   // #327
        for (int k = 0; k < q; k++)
          if (!strcmp(ordenadas[k], chave)) dentro[k] = 1;
    }
  }
  fil_conta_reconciliar(ordenadas, fora, dentro, q);
}
