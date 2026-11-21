// CHAVE DO MOTOR DE LEGENDAS ASS (.tpk, PR 734 etapa D): a ultima linha dos
// Ajustes liga/desliga o motor novo em tempo de execucao. Prova o padrao
// ligado, a volta ao motor antigo, a persistencia no ajustes.txt, a posicao
// append-only no enum e que a chave alimenta ass_novo_ligado() (assperf.h).
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>

int main(void) {
  char dir[256];
  snprintf(dir, sizeof dir, "%s/nuvio-assmotor-XXXXXX",
           getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp");
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1);
  dados_iniciar(dir);

  // Enum: a chave e a ULTIMA (append-only) e fica nesta TV (escopo TV, nunca
  // sobe para a conta nem muda com o perfil).
  assert(AJ_ASS_MOTOR == AJ_N - 1);
  assert(somenteDesteAparelho(AJ_ASS_MOTOR));
  assert(soNaTv(AJ_ASS_MOTOR));
  assert(ajustes_escopo(AJ_ASS_MOTOR) == ESC_TV);
  // E a ultima linha visivel da lista de ajustes (etapa D: no FIM).
  { int k;
    for (k = AJ_N_TELA - 1; k >= 0 && TELA[k].tipo != IT_OPC; k--) ;
    assert(k >= 0 && TELA[k].op == AJ_ASS_MOTOR); }

  // Sem arquivo algum: o padrao de fabrica e o motor novo ligado.
  ajustes_dir(dir);
  assert(valor[AJ_ASS_MOTOR] == 0);
  assert(ajustes_ass_motor() == 1);
  assert(ass_novo_ligado() == 1);            // a chave alimenta o motor

  // Desligado = volta ao motor antigo, em tempo de execucao. A escrita direta
  // no teste publica o espelho atomico como a confirmacao dos Ajustes faz.
  valor[AJ_ASS_MOTOR] = 1;
  assMotorPublicar();
  assert(ajustes_ass_motor() == 0);
  assert(ass_novo_ligado() == 0);

  // E vai para o arquivo como as outras chaves.
  assert(gravar());
  { char *t = dados_ler("ajustes.txt");
    assert(t && strstr(t, "assMotorNovoLocal 1"));
    free(t); }

  // Arranque seguinte: lido do disco, continua no motor antigo.
  valor[AJ_ASS_MOTOR] = 0;
  ajustes_dir(dir);
  assert(ajustes_ass_motor() == 0);
  assert(ass_novo_ligado() == 0);

  // Liga de novo e persiste do mesmo jeito.
  valor[AJ_ASS_MOTOR] = 0;
  assMotorPublicar();
  assert(gravar());
  ajustes_dir(dir);
  assert(ajustes_ass_motor() == 1);
  assert(ass_novo_ligado() == 1);

  printf("assmotor: ok\n");
  return 0;
}
