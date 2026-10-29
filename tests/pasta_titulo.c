// Exercise the real Settings option, default, scope and Home placement.
#include "../src/ajustes.c"
#include <assert.h>

int main(void) {
  int op = -1, row = -1;
  for (int i = 0; i < AJ_N; i++)
    if (!strcmp(CHAVE[i], "heroFolderDetailsLocal")) op = i;
  assert(op >= 0 && "folder hero details toggle must exist");
  assert(valorPadrao[op] == 0 && ajustes_hero_pasta_detalhes());
  assert(somenteDesteAparelho(op));
  assert(OPCOES[op].n == 2);
  assert(!strcmp(OPCOES[op].rotulo, "Mostrar detalhes da pasta no destaque"));
  for (int i = 0; i < AJ_N_TELA; i++)
    if (TELA[i].tipo == IT_OPC && TELA[i].op == op) {
      assert(row == -1);
      row = i;
    }
  assert(row > 0 && TELA[row - 1].op == AJ_HERO_DURACAO);
  valor[op] = 1;
  assert(!ajustes_hero_pasta_detalhes());
  valor[op] = 0;
  assert(ajustes_hero_pasta_detalhes());
  puts("ok: folder hero details default on, toggle off/on, local scope, Home placement");
  return 0;
}
