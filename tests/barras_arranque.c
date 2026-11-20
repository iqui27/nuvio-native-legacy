/* #768: o getter nao pode fixar o padrao Desligado quando a primeira janela
   consulta antes de ajustes_dir() ler o arquivo salvo. Simula a mesma ordem:
   consulta precoce -> "arquivo" muda o ajuste -> consulta da primeira
   reproducao tem de ver o valor salvo. */
#define AJUSTES_TESTE 1
#include "../src/barraspretas.c"
#include <stdio.h>

static int g_aj;               /* o que ajustes_barras_pretas() le de valor[] */

char *dados_ler(const char *name) { (void)name; return NULL; }
int dados_gravar(const char *name, const char *text) { (void)name; (void)text; return 0; }
int ajustes_barras_pretas(void) { return g_aj; }

int main(void) {
  /* valor[] ainda no padrao (Desligado -> ajuste retorna 0) */
  g_aj = 0;
  int janela_inicial = barraspretas_ativo();
  /* ajustes_dir() terminou e o arquivo tinha Ligado */
  g_aj = 1;
  int primeira = barraspretas_ativo();
  barraspretas_invalidar();
  int depois = barraspretas_ativo();
  printf("janela inicial=%d preferencia depois de ler disco=%d primeira reproducao=%d depois de invalidar=%d\n",
         janela_inicial, g_aj, primeira, depois);
  return !(janela_inicial == 0 && primeira == 1 && depois == 1);
}
