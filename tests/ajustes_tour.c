/* "O que o Nuvio faz?": tabela de cartoes do tour. Sem UI, rede ou sync. */
#include "../src/ajustes.c"
#include <assert.h>

static int naTela(int op) {
  for (int i = 0; i < AJ_N_TELA; i++) if (TELA[i].tipo == IT_OPC && TELA[i].op == op) return 1;
  return 0;
}

int main(void) {
  int n = tourN();
  const int required[] = { AJ_LEG_SYNC_AUTO, AJ_LEG_LINGUA2, AJ_SEEKR_LIGADO, AJ_RELOGIO,
    AJ_FONTE_PRIORIDADE, AJ_TAM_MAX, AJ_DTS_AC3, AJ_PERFIL_ATIVO, AJ_PERFIL_PESQ,
    AJ_XTREAM_SERVIDOR, AJ_JF_LIGADO, AJ_ESMAECER, AJ_DV_MKV, -1 };
  for (unsigned k = 0; k < sizeof required / sizeof *required; k++) {
    int found = 0;
    for (int i = 0; i < n; i++) if (tourOp(i) == required[k]) found = 1;
    assert(found && "required functional tour card missing");
  }
  aj4Iniciar(); focoIndice = 1; aj4Linha = 7;
  aj4Evento(SDLK_RETURN);
  assert(tourAberto && "Settings v4 must open functional tour");
  tourSair();
  assert(focoIndice && aj4Linha == 7);
  for (int i = 0; i < n; i++) {
    int op = tourOp(i);
    if (op < 0) {
      assert(!strcmp(TOUR[i].tit, "Assistir Juntos"));
      int before = valor[AJ_SOCIAL];
      tourLigar(i); assert(ajustes_pediu_tela() == GT_BIBLIOTECA);
      tourOnde(i); assert(ajustes_pediu_tela() == GT_BIBLIOTECA);
      assert(valor[AJ_SOCIAL] == before);
      continue;
    }
    assert(op >= 0 && op < AJ_N);          /* cada cartao tem op valido */
    assert(naTela(op));                    /* e a opcao existe na lista de Ajustes */
    assert(tourTitulo(i)[0] && tourFrase(i)[0]);
    for (int j = 0; j < i; j++) assert(tourOp(j) != op);
  }
  /* "Ligar agora": foco na opcao + edicao aberta quando a opcao existe aqui. */
  for (int i = 0; i < n; i++) {
    if (!tourDisponivel(i) || tourOp(i) < 0) continue;
    int op = tourOp(i);
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourLigar(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 1);
    uxAbrirOp = -1; uxAbrirEdicao = 0;
    tourOnde(i);
    assert(uxAbrirOp == tourOp(i) && uxAbrirEdicao == 0);
    tourAtualizar(0);
    int target = op == AJ_XTREAM_SERVIDOR ? AJ_IPTV_LISTA_XTREAM : op;
    assert(!tourAberto && !focoIndice);
    assert(aj4Atual == aj4Secao(target) && aj4Opcoes[aj4Linha] == target);
    assert(!uxEditor);
    int before = valor[op];
    tourLigar(i); tourAtualizar(0);
    if (op == AJ_PERFIL_ATIVO) { assert(aj4Util == 4); aj4Util = 0; }
    else assert(aj4Opcoes[aj4Linha] == target && uxOp == target && uxEditor);
    assert(valor[op] == before);
    uxCancelar();
  }
  /* "Novo" por 30 dias a contar da primeira vez que o tour foi visto. */
  assert(!tourNovoJanela(1000, 999));
  assert(!tourNovoJanela(0, 1000));
  assert(tourNovoJanela(1000, 1000 + 29 * 86400));
  assert(!tourNovoJanela(1000, 1000 + 31 * 86400));
  puts("ok");
  return 0;
}
