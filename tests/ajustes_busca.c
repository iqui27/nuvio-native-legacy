/* Settings search still finds every option on screen: by its label and by
 * its old place ("Caminho antigo"), and by the names it had before the
 * ajustes UX rewrite (renomes below). */
#include "../src/ajustes.c"
#include <stdio.h>

static AjusteBuscaResultado res[AJ_N];

// Labels renamed by the ajustes UX work: the old name must still find it.
static const struct { int op; const char *antigo; } renomes[] = {
  { AJ_FONTE_REGEX, "Filtrar fontes pelo nome (regex)" },
  { AJ_LEG_FORCADA, "Só os letreiros quando o áudio já é no seu idioma" },
  { AJ_PLR_CLASSIF, "Classificação indicativa no player" },
  { AJ_PULAR_CREDITOS, "Pular os créditos automaticamente" },
  { AJ_PROXIMO_AUTO, "Tocar o próximo episódio automaticamente" },
  { AJ_JF_LIGADO, "Servidores pessoais (experimental)" },
  { AJ_HIST_CONTA, "Enviar histórico para a conta Nuvio" },
  { -1, NULL },
};

static int acha(const char *q, int op) {
  int i, n = ajustes_buscar(q, res, AJ_N);
  for (i = 0; i < n; i++) if (res[i].op == op) return 1;
  return 0;
}

int main(void) {
  int i, vezes[AJ_N] = { 0 }, ruins = 0, total = 0;
  for (i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op >= 0 && TELA[i].op < AJ_N) vezes[TELA[i].op]++;
  for (i = 0; i < AJ_N; i++) {
    const char *antigo;
    // the same doors ajustes_buscar keeps shut: not on this TV, or not unlocked
    if (!vezes[i] || !ajOpcaoDisponivel(i)) continue;
    if (i == AJ_ICONE_APP && !apoiador_ativo()) continue;
    if ((i == AJ_PERFIL_PESQ || i == AJ_PERFIL_EDITAR) && !recomenda_ativo()) continue;
    if (i == AJ_BUSCA_CINEMETA && valor[i] == 0 && !cinemetaInstalado()) continue;
    total++;
    if (rotuloOpcao(i)[0] && !acha(rotuloOpcao(i), i)) { printf("FAIL: label \"%s\" (%s) not found\n", rotuloOpcao(i), CHAVE[i]); ruins++; }
    antigo = uxCaminhoAntigo(i);
    if (antigo && antigo[0] && !acha(antigo, i)) { printf("FAIL: old path \"%s\" (%s) not found\n", antigo, CHAVE[i]); ruins++; }
  }
  for (i = 0; renomes[i].antigo; i++)
    if (!acha(renomes[i].antigo, renomes[i].op)) { printf("FAIL: old name \"%s\" (%s) not found\n", renomes[i].antigo, CHAVE[renomes[i].op]); ruins++; }
  if (ruins) return 1;
  printf("PASS: search finds all %d options on screen.\n", total);
  return 0;
}
