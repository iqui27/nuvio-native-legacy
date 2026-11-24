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

int farolacao_addon_isolar(const char *chave, int fontesVivas) {
  int rc;
  farol_registrar_acao(FAROL_A_ISOLAR_ADDON);
  if (!chave || !*chave) rc = FAAD_INVALIDO;
  else if (addonstats_isolado_s(chave) > 0) rc = FAAD_JA_ISOLADO;
  else if (fontesVivas <= 1) rc = FAAD_UNICO;
  else {
    // O teto e conferido DENTRO da trava de addonstats: dois fios nunca passam do 3.
    int r = addonstats_isolar_teto(chave, FAAD_ISOLA_S, FAAD_TETO_ISOLADOS);
    rc = r == 1 ? FAAD_OK : r == -1 ? FAAD_JA_ISOLADO : r == -2 ? FAAD_TETO : FAAD_INVALIDO;
  }
  if (rc == FAAD_OK) addonstats_isolar_salvar();
  farol_registrar_resultado(FAROL_A_ISOLAR_ADDON, rc == FAAD_OK || rc == FAAD_JA_ISOLADO);
  return rc;
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
  int rc = farolacao_addon_isolar(chave, addons_fontes_vivas_n());
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
