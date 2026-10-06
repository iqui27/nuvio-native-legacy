// Liga o Plex como origem extra de fontes do addons.c (um arquivo a parte para
// plex.c compilar e ser testado sem arrastar o modulo de addons).
#include "addons.h"
#include "badges.h"
#include "plex.h"
#include <stdio.h>

static int origem(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                  OrigemAviso aviso, void *avisoU, void *saida) {
  Stream **l = (Stream **)saida;
  int n, i;
  (void)aviso; (void)avisoU;   // one quiet lookup, no per-source progress rows
  n = plex_consultar(id, tipo, NULL, 0, cancelado, ctx, l);
  // Same badges as every addon source: classified once from the text the
  // server's real media streams produced (resolution, HDR, codecs, audio).
  for (i = 0; i < n && *l; i++) {
    char t[2400];
    snprintf(t, sizeof t, "%s %s", (*l)[i].rotulo, (*l)[i].descricao);
    (*l)[i].badges = badges_detectar(t);
  }
  return n;
}

void plex_ligar_aos_addons(void) { addons_definir_origem_extra(origem, plex_casamento_ativo); }
