// Stubs for tests that link descoberta.c / app.c without servidores.c and a real
// ajustes.c: no personal server (Jellyfin, Emby or Plex) is connected, so no
// server row or page exists.
#ifndef NV_JELLYFIN_STUB_INC
#define NV_JELLYFIN_STUB_INC
#include "catalogo.h"
int ajustes_jellyfin_ligado(void) { return 0; }
int servidores_conectado(void) { return 0; }
int servidores_chave_fileira(const char *chave) { (void)chave; return 0; }
int servidores_fileiras_copiar(CatItem *i, int mi, CatFileira *f, int mf, int *n) {
  (void)i; (void)mi; (void)f; (void)mf; if (n) *n = 0; return 0; }
int servidores_pagina(const char *c, int ini, int lim, CatItem *out, int max, int *total) {
  (void)c; (void)ini; (void)lim; (void)out; (void)max; if (total) *total = 0; return -1; }
int servidores_ficha(CatItem *item, CatEp *eps, int maxEps) { (void)item; (void)eps; (void)maxEps; return -1; }
unsigned servidores_fileiras_versao(void) { return 0; }
int servidores_passo(void) { return 0; }
#endif
