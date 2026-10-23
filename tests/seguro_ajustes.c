// Modo seguro de ponta a ponta com o ajustes.c de verdade e discos de verdade:
// cada FASE e um processo novo (o teste se re-executa), porque o que se quer
// provar e o que sobrevive a um processo que morre sem se despedir (_exit, como
// o kill -9 na LG). Uso: seguro_ajustes <pasta> <fase>; o .sh encadeia as fases.
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

#define OK(c) do { if (!(c)) { fprintf(stderr, "FALHOU fase %s: %s (linha %d)\n", fase, #c, __LINE__); exit(1); } } while (0)

static const char *fase;
static void sobe_ate_fileiras(int alvo) { while (riscoAtual(AJ_FIL_LIMITE) < alvo) mudarValor(AJ_FIL_LIMITE, 1); }

int main(int argc, char **argv) {
  const char *dir;
  if (argc < 3) return 2;
  dir = argv[1]; fase = argv[2];
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);
  ajustes_dir(dir);
  // O veredito "caiu" e o do avisos.c; aqui vem da fase.
  { int caiu = strstr(fase, "caiu") != NULL;
    ajustes_seguro_iniciar(caiu); }

  if (!strcmp(fase, "1-limites")) {
    // Folha: subir de 7 a 16 nao pergunta; o 17 abre a folha e NAO muda nada.
    sobe_ate_fileiras(16);
    OK(riscoAtual(AJ_FIL_LIMITE) == 16 && !riscoFolha);
    mudarValor(AJ_FIL_LIMITE, 1);
    OK(riscoFolha == SEG_AVISO_FILEIRAS && riscoAtual(AJ_FIL_LIMITE) == 16);
    riscoFolhaEvento(SDLK_ESCAPE);                    // cancelar: fica em 16
    OK(!riscoFolha && riscoAtual(AJ_FIL_LIMITE) == 16 && !seguro_aviso_visto(SEG_AVISO_FILEIRAS));
    mudarValor(AJ_FIL_LIMITE, 1);
    riscoFolhaEvento(SDLK_LEFT);                      // foco em Continuar
    riscoFolhaEvento(SDLK_RETURN);
    OK(riscoAtual(AJ_FIL_LIMITE) == 17 && seguro_aviso_visto(SEG_AVISO_FILEIRAS));
    mudarValor(AJ_FIL_LIMITE, 1);                     // depois de vista: sem folha
    OK(!riscoFolha && riscoAtual(AJ_FIL_LIMITE) == 18);
    // Teto: 40 e nao passa.
    sobe_ate_fileiras(40); mudarValor(AJ_FIL_LIMITE, 1);
    OK(riscoAtual(AJ_FIL_LIMITE) == 40 && fil_limite() == 40);
    // Itens: 12 -> 18 abre folha propria.
    mudarValor(AJ_ITENS_FILEIRA, 1);
    OK(riscoFolha == SEG_AVISO_ITENS && valor[AJ_ITENS_FILEIRA] == 0);
    riscoFolhaEvento(SDLK_LEFT); riscoFolhaEvento(SDLK_RETURN);
    OK(valor[AJ_ITENS_FILEIRA] == 1 && ajustes_itens_fileira() == 18);
    OK(seguro_n_provisorias() == 2);
    _exit(0);                                         // MORRE sem se despedir
  }
  if (!strcmp(fase, "2-caiu-reverte")) {
    // A sessao anterior caiu com fileiras 40 e itens 18 em prova.
    OK(fil_limite_gravado() == FIL_LIMITE_PADRAO);    // 7
    OK(valor[AJ_ITENS_FILEIRA] == 0 && ajustes_itens_fileira() == 12);
    OK(avisos_n_novos() == 2);                        // um aviso por ajuste desfeito
    OK(!seguro_perfil_ativo());
    // De novo: sobe, fica 3 min (batida), cai: NAO desfaz.
    sobe_ate_fileiras(30);
    seguro_batida(SEG_CONFIRMA_S + 30);
    OK(seguro_n_provisorias() == 0);
    _exit(0);
  }
  if (!strcmp(fase, "3-caiu-confirmada")) {
    OK(fil_limite_gravado() == 30);
    OK(avisos_n_novos() == 0);
    // Liga vidro e 4K, sai limpo (confirma).
    ajustes_definir_vidro(1);
    { int a = valor[AJ_RESOLUCAO]; valor[AJ_RESOLUCAO] = RES_4K; gravar(); riscoNotar(AJ_RESOLUCAO, a); }
    OK(seguro_n_provisorias() == 2);
    seguro_encerrar();
    _exit(0);
  }
  if (!strcmp(fase, "4-limpa")) {
    OK(ajustes_vidro() && ajustes_4k() && fil_limite() == 30);
    // Sessao que cai rapido, sem nada em prova (1a queda).
    _exit(0);
  }
  if (!strcmp(fase, "5-caiu")) { OK(ajustes_vidro() && ajustes_4k()); _exit(0); }            // 2a queda rapida
  if (!strcmp(fase, "6-caiu-perfil")) {
    // Perfil seguro: acessores devolvem o seguro, o ARQUIVO nao mudou.
    OK(seguro_perfil_ativo());
    OK(!ajustes_vidro() && !ajustes_4k() && fil_limite() == FIL_LIMITE_VIGIADO);
    OK(valor[AJ_VIDRO] == 0 && valor[AJ_RESOLUCAO] == RES_4K && fil_limite_gravado() == 30);
    OK(avisos_n_novos() == 1);
    // Editar durante o perfil seguro mostra e grava o valor REAL, nunca o teto.
    ajustes_definir_p2p_ligado(0);
    { char *t = dados_ler("ajustes.txt"); OK(t && strstr(t, "vidroLocal 0")); free(t); }
    _exit(0);                                         // cai de novo, ainda no perfil seguro
  }
  if (!strcmp(fase, "7-caiu-persiste")) {
    OK(!seguro_perfil_ativo());
    OK(!ajustes_vidro() && !ajustes_4k() && fil_limite() == FIL_LIMITE_PADRAO);
    OK(valor[AJ_VIDRO] == 1 && valor[AJ_RESOLUCAO] == 0);   // gravados de vez
    { char *t = dados_ler("ajustes.txt"); OK(t && strstr(t, "vidroLocal 1") && strstr(t, "p2pLocal 1")); free(t); }
    seguro_encerrar();
    _exit(0);
  }
  fprintf(stderr, "fase desconhecida: %s\n", fase);
  return 2;
}
