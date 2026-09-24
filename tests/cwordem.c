// A REGRA DE ORDENACAO de "Continuar assistindo" (issue #127), sem rede nem
// tela: src/cwordem.c sozinho. A montagem (descoberta.c) e a fileira separada
// (home.c) estao em tests/cwordem_desc.c e tests/cwordem_home.c.
//
//   bash tests/cwordem.sh
#include "../src/cwordem.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define DIA (24LL * 60 * 60 * 1000)
static const long long AGORA = 1790000000000LL;

int main(void) {
  // Chegam na ordem por instante (o mais recente primeiro), como
  // montarContinuar entrega:
  //   0 em andamento
  //   1 a seguir, estreia daqui a 5 dias       -> futuro
  //   2 a seguir, ja foi ao ar ontem            -> exibido
  //   3 a seguir, estreia amanha                -> futuro
  //   4 a seguir, sem data                      -> exibido (web: hasAired !== false)
  //   5 em andamento com data futura (nao e "a seguir") -> exibido
  CwoItem v[6] = {
    { 0, CWO_SEM_DATA },
    { 1, AGORA + 5 * DIA },
    { 1, AGORA - DIA },
    { 1, AGORA + DIA },
    { 1, CWO_SEM_DATA },
    { 0, AGORA + 3 * DIA },
  };
  int perm[6], p, i;

  p = cwo_ordenar(v, 6, CWO_PADRAO, AGORA, perm);
  assert(p == 6);
  for (i = 0; i < 6; i++) assert(perm[i] == i);
  puts("ok  padrao: mais recente primeiro, nada se move");

  { static const int esperado[6] = { 0, 2, 4, 5, 3, 1 };
    p = cwo_ordenar(v, 6, CWO_STREAMING, AGORA, perm);
    assert(p == 4);
    for (i = 0; i < 6; i++) assert(perm[i] == esperado[i]);
    puts("ok  streaming: exibidos pelo instante, depois futuros pela estreia");
    p = cwo_ordenar(v, 6, CWO_SEPARAR, AGORA, perm);
    assert(p == 4);
    for (i = 0; i < 6; i++) assert(perm[i] == esperado[i]);
    puts("ok  separar: mesma ordem, os 2 futuros depois da parte principal"); }

  // Estreia exatamente agora ja foi ao ar (`released <= Date.now()`).
  { CwoItem x = { 1, AGORA };
    assert(!cwo_futuro(&x, AGORA)); x.estreiaMs = AGORA + 1; assert(cwo_futuro(&x, AGORA)); }
  // Empate de estreia: ordem de entrada.
  { CwoItem e[3] = { { 1, AGORA + DIA }, { 1, AGORA + DIA }, { 0, CWO_SEM_DATA } };
    p = cwo_ordenar(e, 3, CWO_SEPARAR, AGORA, perm);
    assert(p == 1 && perm[0] == 2 && perm[1] == 0 && perm[2] == 1); }
  assert(cwo_ordenar(v, 0, CWO_SEPARAR, AGORA, perm) == 0);
  puts("ok  bordas: estreia agora = exibido, empate estavel, lista vazia");

  // Tabela de estreias.
  assert(cwo_estreia("tt1:1:2") == CWO_SEM_DATA);
  cwo_marcar_estreia("tt1:1:2", AGORA);
  cwo_marcar_estreia("tt1:1:2", AGORA + DIA);   // regravar troca, nao duplica
  assert(cwo_estreia("tt1:1:2") == AGORA + DIA);
  { char id[32];
    for (i = 0; i < 200; i++) { snprintf(id, sizeof id, "tt9:%d:1", i); cwo_marcar_estreia(id, i); }
    assert(cwo_estreia("tt9:199:1") == 199);   // cheia: a mais nova fica
    assert(cwo_estreia("tt9:0:1") == CWO_SEM_DATA); }
  puts("ok  estreias: regravar troca, tabela cheia gira");

  { const char *ids[2] = { "tt5:2:1", "tt6:1:9" };
    cwo_publicar_futuros(ids, 2);
    assert(cwo_e_futuro("tt5:2:1") && cwo_e_futuro("tt6:1:9") && !cwo_e_futuro("tt5:2:2"));
    cwo_publicar_futuros(NULL, 0);
    assert(!cwo_e_futuro("tt5:2:1")); }
  puts("ok  futuros publicados: substitui o conjunto inteiro");
  puts("cwordem: tudo ok");
  return 0;
}
