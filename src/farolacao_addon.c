// FAROL #17 — ver farolacao_addon.h.
#include "farolacao_addon.h"
#include "addons.h"
#include "addonstats.h"
#include "farol.h"
#include "faroltexto.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include <stdio.h>
#include <string.h>

int farolacao_addon_isolar_entre(const char *chave, const char (*fontes)[ADDONSTATS_CHAVE_N], int nFontes) {
  int rc;
  farol_registrar_acao(FAROL_A_ISOLAR_ADDON);
  if (!chave || !*chave) rc = FAAD_INVALIDO;
  else {
    // Teto E ultima fonte conferidos DENTRO da trava de addonstats, junto com a
    // gravacao: dois fios nunca passam do 3 nem deixam zero fontes vivas.
    int r = addonstats_isolar_guarda(chave, FAAD_ISOLA_S, FAAD_TETO_ISOLADOS, fontes, nFontes < 0 ? 0 : nFontes);
    rc = r == 1 ? FAAD_OK : r == -1 ? FAAD_JA_ISOLADO : r == -2 ? FAAD_TETO
       : r == -3 ? FAAD_UNICO : FAAD_INVALIDO;
  }
  if (rc == FAAD_OK) addonstats_isolar_salvar();
  farol_registrar_resultado(FAROL_A_ISOLAR_ADDON, rc == FAAD_OK || rc == FAAD_JA_ISOLADO);
  return rc;
}

_Static_assert(FAAD_FONTES_MAX >= ADDONS_MAX, "cabem todos os add-ons da lista");

int farolacao_addon_isolar(const char *chave) {
  char fontes[FAAD_FONTES_MAX][ADDONSTATS_CHAVE_N];
  int n = addons_fontes_chaves(fontes, FAAD_FONTES_MAX);
  return farolacao_addon_isolar_entre(chave, (const char (*)[ADDONSTATS_CHAVE_N])fontes, n);
}

int farolacao_addon_pode_isolar(void) {
  return addons_lento_isolavel() >= 0 && addons_fontes_vivas_n() > 1 &&
         addonstats_isolados_n() < FAAD_TETO_ISOLADOS;
}

int farolacao_addon_desfazer(const char *chave) {
  int ok = addonstats_desisolar(chave);
  if (ok) addonstats_isolar_salvar();
  return ok;
}

int farolacao_addon_isolado(const char *chave, int *restamS) {
  int s = addonstats_isolado_s(chave);
  if (restamS) *restamS = s;
  return s > 0;
}

int farolacao_addon_isolados_n(void) { return addonstats_isolados_n(); }

int farolacao_addon_pedido(int acaoFarol) {
  int p;
  switch (acaoFarol) {
    case FAROL_A_ABRIR_FONTES:      p = FAAD_PEDIU_FONTES; break;
    case FAROL_A_ABRIR_DEBRID:
    case FAROL_A_RECONECTAR_DEBRID: p = FAAD_PEDIU_DEBRID; break;
    default: return FAAD_PEDIU_NADA;
  }
  farol_registrar_acao(acaoFarol);
  return p;
}

void farolacao_addon_resultado(int acaoFarol, int ok) {
  if (acaoFarol == FAROL_A_ABRIR_FONTES || acaoFarol == FAROL_A_ABRIR_DEBRID ||
      acaoFarol == FAROL_A_RECONECTAR_DEBRID)
    farol_registrar_resultado(acaoFarol, ok);
}

typedef struct { char chave[48]; } CtxDesfazer;

static void ilhaDesfazer(const void *ctx) {
  farolacao_addon_desfazer(((const CtxDesfazer *)ctx)->chave);
}

int farolacao_addon_isolar_lento(void) {
  char chave[32];
  int i = addons_lento_isolavel();
  if (i < 0 || !addons_chave_estatistica(i, chave)) return FAAD_NENHUM;
  int rc = farolacao_addon_isolar(chave);
  if (rc == FAAD_OK) {
    CtxDesfazer c;
    IlhaAcao a;
    memset(&c, 0, sizeof c);
    snprintf(c.chave, sizeof c.chave, "%s", chave);
    memset(&a, 0, sizeof a);
    a.frase = i18n(FT_ADDON_ISOLADO);
    a.titulo = addons_nome(i);
    a.onde = i18n(FT_ADDON_DE_LADO);
    a.tipo = ILHA_INFO;
    a.desfazer = ilhaDesfazer;
    a.ctx = &c; a.ctxN = sizeof c;
    ilhaacao_feita(&a);
  }
  return rc;
}
