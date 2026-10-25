// Liga o Plex como origem extra de fontes do addons.c (um arquivo a parte para
// plex.c compilar e ser testado sem arrastar o modulo de addons).
#include "addons.h"
#include "plex.h"

static int origem(const char *id, const char *tipo, int (*cancelado)(void *), void *ctx,
                  OrigemAviso aviso, void *avisoU, void *saida) {
  (void)aviso; (void)avisoU;   // one quiet lookup, no per-source progress rows
  return plex_consultar(id, tipo, NULL, 0, cancelado, ctx, (Stream **)saida);
}

static void *preparoCapturar(void) { return plex_preparo_capturar(); }
static int preparoAtual(const void *p) { return plex_preparo_atual(p); }
static int preparoConsultar(const void *p, const char *id, const char *tipo,
                            int (*cancelado)(void *), void *ctx,
                            OrigemAviso aviso, void *avisoU, Stream **saida) {
  (void)aviso; (void)avisoU;
  return plex_preparo_consultar(p, id, tipo, cancelado, ctx, saida);
}
static void preparoLiberar(void *p) { plex_preparo_liberar(p); }
void plex_ligar_aos_addons(void) {
  static const OrigemPreparoOps ops = { preparoCapturar, preparoAtual, preparoConsultar, preparoLiberar };
  addons_definir_origem_extra(origem, plex_casamento_ativo);
  addons_definir_origem_extra_preparo(origem, &ops);
}
