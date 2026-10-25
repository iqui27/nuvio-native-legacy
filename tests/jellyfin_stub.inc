// Stubs for tests that link descoberta.c / app.c without servidores.c and a real
// ajustes.c: no personal server (Jellyfin, Emby or Plex) is connected, so no
// server row or page exists.
#ifndef NV_JELLYFIN_STUB_INC
#define NV_JELLYFIN_STUB_INC
#include "catalogo.h"
#include "addons.h"
#include <stdint.h>
int ajustes_jellyfin_ligado(void) { return 0; }
int servidores_conectado(void) { return 0; }
int servidores_continuar(CatItem *out, int max) { (void)out; (void)max; return 0; }
int servidores_chave_fileira(const char *chave) { (void)chave; return 0; }
int servidores_fileiras_copiar(CatItem *i, int mi, CatFileira *f, int mf, int *n) {
  (void)i; (void)mi; (void)f; (void)mf; if (n) *n = 0; return 0; }
int servidores_pagina(const char *c, int ini, int lim, CatItem *out, int max, int *total, int *raw) {
  (void)c; (void)ini; (void)lim; (void)out; (void)max; if (total) *total = 0; if (raw) *raw = 0; return -1; }
int servidores_ficha(CatItem *item, CatEp *eps, int maxEps) { (void)item; (void)eps; (void)maxEps; return -1; }
unsigned servidores_fileiras_versao(void) { return 0; }
int servidores_passo(void) { return 0; }

// Isolated discovery fixtures do not retry account writes; real modules override.
__attribute__((weak)) int contapend_reenviar(void) { return 0; }

#ifndef NV_TEST_SERVIDORES_APENAS
// Mach-O weak_import em descoberta.c: fixtures isolados precisam de definicoes
// fortes para linkar no macOS (ver perfisessao.h).
#ifndef NV_TEST_SESSAO_PROPRIA
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t g) { (void)g; return 1; }
int perfis_sessao_travar(uint64_t g) { (void)g; return 1; }
void perfis_sessao_destravar(void) {}
#endif
#ifndef NV_TEST_ADDONS_PROPRIOS
int addons_copiar_identidades(AddonIdentidade *a, int m, unsigned v, int p) {
  (void)a; (void)m; (void)v; (void)p; return -1; }
int addons_travar_lista(unsigned v, int p) { (void)v; (void)p; return 0; }
void addons_destravar_lista(void) {}
int fil_podar_catalogos_de(const char *const *i, const char *const *b, int n, int p, unsigned v) {
  (void)i; (void)b; (void)n; (void)p; (void)v; return 0; }
#endif
#ifndef NV_TEST_SOCIAL_PROPRIO
extern int trakt_enfeitar_lote(CatItem *, int) __attribute__((weak));
extern int recomenda_social_mesclar(CatItem *, int, int) __attribute__((weak));
int trakt_enfeitar_lote_sessao(CatItem *s, int n, uint64_t sessao) {
  (void)sessao;
  return trakt_enfeitar_lote ? trakt_enfeitar_lote(s, n) : n;
}
int recomenda_social_mesclar_sessao(CatItem *i, int nTrakt, int max, uint64_t sessao) {
  (void)sessao;
  return recomenda_social_mesclar ? recomenda_social_mesclar(i, nTrakt, max) : nTrakt;
}
#endif
#endif
#endif
