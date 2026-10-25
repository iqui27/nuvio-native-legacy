/* M3: an option that greys out (inativa) says why in its own row, and when it
 * hangs on another option, that mother is on screen to go to. Each option in
 * TELA is moved through its values one at a time from the defaults. */
#include "../src/ajustes.c"
#include <stdio.h>
#include <assert.h>

static int naTela[AJ_N], semMotivo[AJ_N], maeLonge[AJ_N];

static int confere(int mae, int v) {
  int i, ruins = 0;
  for (i = 0; i < AJ_N; i++) {
    int req;
    if (!naTela[i] || !ajOpcaoDisponivel(i) || !inativa(i)) continue;
    req = uxRequisito(i);
    if (!strcmp(ajudaOpcaoBase(i), "Indisponível com os ajustes atuais.")) {
      if (!semMotivo[i]++) printf("FAIL: %s greys out (%s=%d) with no reason\n", CHAVE[i], mae >= 0 ? CHAVE[mae] : "defaults", v);
      ruins++;
    }
    if (req >= 0 && (!naTela[req] || !ajOpcaoDisponivel(req))) {
      if (!maeLonge[i]++) printf("FAIL: %s needs %s, which is not on screen\n", CHAVE[i], CHAVE[req]);
      ruins++;
    }
  }
  return ruins;
}

int main(void) {
  int i, v, ruins = 0, casos = 0;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op >= 0 && TELA[i].op < AJ_N) naTela[TELA[i].op] = 1;
  for (i = 0; i < AJ_N; i++) valor[i] = valorPadrao[i];
  // The dependency offer must leave values untouched until OK is pressed.
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  valor[AJ_CW_LIGADO] = 0; valor[AJ_CW_THUMB] = 1; valor[AJ_CW_BLUR_PROX] = 1;
  { int salvo[AJ_N]; char aviso[400];
    memcpy(salvo, valor, sizeof salvo);
    assert(uxLigarRequisitos(AJ_CW_BLUR_PROX, 0) == 2);
    assert(!memcmp(salvo, valor, sizeof salvo));
    ajAvisoTexto(AJ_CW_BLUR_PROX, aviso, sizeof aviso);
    assert(strstr(aviso, "OK liga as duas"));
  }
  for (i = 0; i < AJ_N; i++) valor[i] = valorPadrao[i];
  assert(uxRequisito(AJ_MANTER_PRAZO) == AJ_MANTER_VIDEO);
  assert(uxRequisito(AJ_DESCANSO_ESTILO) == AJ_ESMAECER);
  assert(uxRequisito(AJ_EP_TAMANHO) == AJ_HOME_LAYOUT);
  ruins += confere(-1, 0);
  for (i = 0; i < AJ_N; i++) {
    if (!naTela[i] || !ajOpcaoDisponivel(i) || OPCOES[i].tipo != OP_ESCOLHA || OPCOES[i].n > 12) continue;
    for (v = 0; v < OPCOES[i].n; v++) {
      int salvo = valor[i];
      valor[i] = v; casos++;
      ruins += confere(i, v);
      valor[i] = salvo;
    }
  }
  if (ruins) return 1;
  printf("PASS: every greyed-out option says why (%d states).\n", casos);
  return 0;
}
