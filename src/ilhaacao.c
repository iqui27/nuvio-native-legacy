// Acao feita na ilha — ver ilhaacao.h.
#include "ilhaacao.h"
#include "ilha.h"
#include "idioma.h"
#include "tex_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static IlhaDesfazer ultFn;
static void *ultCtx;

int ilhaacao_tem_desfazer(void) { return ultFn != NULL; }

void ilhaacao_abrir_sessao(void) {
  free(ultCtx); ultCtx = NULL; ultFn = NULL;
}

void ilhaacao_feita(const IlhaAcao *a) {
  static IlhaModal m;
  if (!a || !a->frase || !a->frase[0]) return;
  free(ultCtx); ultCtx = NULL; ultFn = NULL;
  if (a->desfazer) {
    if (a->ctx && a->ctxN) {
      ultCtx = malloc(a->ctxN);
      if (ultCtx) { memcpy(ultCtx, a->ctx, a->ctxN); ultFn = a->desfazer; }
    } else ultFn = a->desfazer;
  }
  memset(&m, 0, sizeof m);
  snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Feito"));
  snprintf(m.titulo, sizeof m.titulo, "%s", a->titulo && a->titulo[0] ? a->titulo : a->frase);
  if (a->titulo && a->titulo[0]) snprintf(m.linha, sizeof m.linha, "%s", a->frase);
  if (a->onde) snprintf(m.nota, sizeof m.nota, "%s", a->onde);
  if (a->arte && a->arte[0] && !tex_falhou(a->arte)) {
    snprintf(m.arte, sizeof m.arte, "%s", a->arte);
    tex_obter_larg(a->arte, 480.0f);   // aquece: o modal abre depois do voo
  } else if (a->thumb && a->thumb[0]) snprintf(m.arte, sizeof m.arte, "%s", a->thumb);
  else snprintf(m.icone, sizeof m.icone, "%s", a->icone && a->icone[0] ? a->icone : "check");
  m.tipo = a->tipo;
  if (ultFn) {
    snprintf(m.botao[m.nBotoes], sizeof m.botao[0], "%s", i18n("Desfazer"));
    snprintf(m.botaoIcone[m.nBotoes++], sizeof m.botaoIcone[0], "aj_rotate-cw");
  }
  snprintf(m.botao[m.nBotoes++], sizeof m.botao[0], "%s", i18n("Ok"));
  ilha_acao(a->icone, a->frase, a->tipo, a->voar ? a->thumb : NULL, NULL, &m);
}

void ilhaacao_botao(int botao) {
  if (botao == 1 && ultFn) {
    IlhaDesfazer fn = ultFn;
    void *c = ultCtx;
    ultFn = NULL; ultCtx = NULL;   // uma vez so
    fn(c);
    free(c);
    ilha_acao("aj_rotate-cw", i18n("Desfeito"), ILHA_INFO, NULL, NULL, NULL);
  }
}
