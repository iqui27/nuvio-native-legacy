// O que addons_carregar tira de addons.txt fica USAVEL?
//
// Este teste existe por um defeito mudo: a leitura do arquivo preenchia nome,
// base e as tres capacidades e NAO preenchia `ativo`. O vetor de addons e
// estatico, nasce zerado, entao todo addon do arquivo ficava desligado — e
// desligado, em addons.c, quer dizer "nao consultado" por fonte, por legenda e
// por catalogo. Na tela ele aparecia; na busca de fontes ele nao existia, sem
// uma linha de log dizendo por que. E a forma extrema do sintoma da issue #83
// ("certain addons not showing").
//
// So o leitor: nada aqui vai a rede.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "addons.h"

static int falhas;

static void conferir(const char *o_que, int obtido, int esperado) {
  if (obtido == esperado) return;
  printf("FALHOU %s: obtido %d, esperado %d\n", o_que, obtido, esperado);
  falhas++;
}

static void conferirTexto(const char *o_que, const char *obtido, const char *esperado) {
  if (!strcmp(obtido, esperado)) return;
  printf("FALHOU %s: obtido \"%s\", esperado \"%s\"\n", o_que, obtido, esperado);
  falhas++;
}

int main(void) {
  char dir[] = "/tmp/nuvio-addonslista-XXXXXX";
  char caminho[600];
  FILE *f;
  int n;

  if (!mkdtemp(dir)) { printf("FALHOU: sem diretorio temporario\n"); return 1; }
  snprintf(caminho, sizeof caminho, "%s/addons.txt", dir);
  f = fopen(caminho, "w");
  if (!f) { printf("FALHOU: sem %s\n", caminho); return 1; }
  // Uma linha por forma que o arquivo aceita: com as tres colunas de
  // capacidade, e sem elas (formato antigo, em que tudo vale 1 menos legenda).
  fprintf(f, "# comentario ignorado\n");
  fprintf(f, "So legenda\thttps://opensubtitles-v3.strem.io\t0\t0\t1\n");
  fprintf(f, "Fonte e catalogo\thttps://exemplo.test/manifest.json\t1\t1\t0\n");
  fprintf(f, "Formato antigo\thttps://antigo.test\n");
  fclose(f);

  n = addons_carregar(dir);
  conferir("addons lidos", n, 3);

  // O QUE ESTE TESTE GUARDA: os tres nascem LIGADOS.
  conferir("ativo[0]", addons_ativo(0), 1);
  conferir("ativo[1]", addons_ativo(1), 1);
  conferir("ativo[2]", addons_ativo(2), 1);

  // E as capacidades continuam vindo das colunas, como antes.
  conferir("fonte[0]",   addons_fornece(0, ADD_STREAM),   0);
  conferir("legenda[0]", addons_fornece(0, ADD_LEGENDA), 1);
  conferir("fonte[1]",   addons_fornece(1, ADD_STREAM),   1);
  conferir("catalogo[1] (ativo e catalogo)", addons_tem_catalogo(1), 1);
  // Formato antigo: fonte e catalogo valem 1, legenda 0.
  conferir("fonte[2]",   addons_fornece(2, ADD_STREAM),   1);
  conferir("legenda[2]", addons_fornece(2, ADD_LEGENDA), 0);

  // A base sai sem "/manifest.json" — a mesma regra de baseNormalizada.
  conferirTexto("base[1]", addons_base(1), "https://exemplo.test");
  conferirTexto("nome[1]", addons_nome(1), "Fonte e catalogo");

  remove(caminho);
  rmdir(dir);
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("addonslista: ok\n");
  return 0;
}
