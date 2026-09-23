// Registro no hub LS2 (lsregistro.c): decodificacao do codigo e politica de
// nova tentativa. Registros 1720-1774 (webOS 4, 964 MB): 1327 LSRegister
// recusados com code=4294966269, um a cada 15 s pela sessao inteira.
#include "lsregistro.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define OK(s) fprintf(stderr, "ok  %s\n", s)

int main(void) {
  LsRegEstado e;
  unsigned t = 1000;
  int n, i;

  // 4294966269 lido como int e -1027: PERMISSION, nao DUPLICATE_NAME
  assert((int)4294966269u == -1027);
  assert(!strcmp(lsreg_nome_codigo((int)4294966269u), "PERMISSION"));
  assert(!strcmp(lsreg_nome_codigo(-1028), "DUPLICATE_NAME"));
  assert(lsreg_nome_codigo(0) == NULL && lsreg_nome_codigo(-1) == NULL);
  assert(lsreg_recusa_da_sessao(-1027) && !lsreg_recusa_da_sessao(-1028));
  OK("code=4294966269 e LS_ERROR_CODE_PERMISSION (-3 - 1024)");

  // AUTOMATICO (trailer) com PERMISSION: 2 tentativas e desiste — antes eram
  // ilimitadas. Simula 30 min de tela chamando a cada quadro de 16 ms.
  memset(&e, 0, sizeof e);
  for (n = 0, i = 0; i < 30 * 60 * 60; i++, t += 16)
    if (lsreg_pode_tentar(&e, t, 1)) { n++; lsreg_falhou(&e, -1027, t); }
  assert(n == 2);
  assert(lsreg_desistiu(&e));
  OK("trailer: PERMISSION para depois de 2 tentativas em 30 min (eram ~120)");

  // pedido da pessoa (play) sempre tenta
  assert(lsreg_pode_tentar(&e, t, 0));
  OK("play continua tentando depois da desistencia do automatico");

  // recusa que pode passar (nome ainda preso apos deploy): 5, com recuo
  memset(&e, 0, sizeof e); t = 5000;
  assert(lsreg_pode_tentar(&e, t, 1));
  lsreg_falhou(&e, -1028, t);
  assert(!lsreg_pode_tentar(&e, t + 2999, 1));
  assert(lsreg_pode_tentar(&e, t + 3000, 1));
  for (n = 1, i = 0; i < 30 * 60 * 60; i++, t += 16)
    if (lsreg_pode_tentar(&e, t, 1)) { n++; lsreg_falhou(&e, -1028, t); }
  assert(n == 5 && lsreg_desistiu(&e));
  OK("DUPLICATE_NAME: recuo 3/8/15/30 s e teto de 5");

  // sucesso zera tudo
  lsreg_deu_certo(&e);
  assert(!lsreg_desistiu(&e) && lsreg_pode_tentar(&e, t, 1));
  // volta do relogio (49 dias): nao trava
  memset(&e, 0, sizeof e);
  lsreg_falhou(&e, -1028, 0xFFFFF000u);
  assert(!lsreg_pode_tentar(&e, 0xFFFFF100u, 1));
  assert(lsreg_pode_tentar(&e, 0x00001000u, 1));
  OK("sucesso zera; volta do SDL_GetTicks nao trava o recuo");

  puts("lsregistro: tudo ok");
  return 0;
}
