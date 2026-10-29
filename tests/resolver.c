// Regra do Resolver (2.1): ordem dos testes, parada na primeira falha, ultimo
// erro, registros enviados por 30 dias. Sem SDL nem rede.
#include "resolverregra.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static ResolverFatos bons(void) {
  ResolverFatos f;
  memset(&f, 0, sizeof f);
  f.addonsQtd = 3;
  return f;
}

int main(void) {
  ResolverFatos f = bons();
  ResolverResultado r;
  ResolverEnviado v[RES_ENVIADOS_MAX];
  ResolverFalha fa;
  char buf[512], cod[16];
  int n, h;
  long agora = 1000000000L;

  // Tudo bem + nenhum erro: nenhuma falha, tudo da cadeia fica OK.
  resolverregra_testar(RES_S_NAO_COMECA, &f, &r);
  assert(r.primeira == -1 && r.acao == RES_ACAO_NENHUMA);
  for (n = 0; n < RES_T_N; n++) assert(r.testado[n] && r.estado[n] == RES_OK);

  // Para na primeira falha: sem internet, o resto nao e testado.
  f.offline = 1; f.debridMotivo[0] = 'x'; f.haFalha = 1;
  resolverregra_testar(RES_S_NAO_COMECA, &f, &r);
  assert(r.primeira == RES_T_INTERNET && r.acao == RES_ACAO_REPETIR);
  assert(r.estado[RES_T_INTERNET] == RES_FALHOU);
  for (n = 1; n < RES_T_N; n++) assert(r.estado[n] == RES_NAO_TESTADO);

  // Internet ok, debrid sem plano antes da fonte: a ordem manda.
  f.offline = 0;
  resolverregra_testar(RES_S_NAO_COMECA, &f, &r);
  assert(r.primeira == RES_T_DEBRID && r.acao == RES_ACAO_FONTES);
  assert(r.estado[RES_T_ADDONS] == RES_OK && r.estado[RES_T_FONTE] == RES_NAO_TESTADO);

  // Sem add-ons: falha no teste 2.
  f = bons(); f.addonsQtd = 0;
  resolverregra_testar(RES_S_NAO_COMECA, &f, &r);
  assert(r.primeira == RES_T_ADDONS && r.acao == RES_ACAO_FONTES);

  // Fonte com erro recente, TV recusando depois: a fonte vem primeiro.
  f = bons(); f.haFalha = 1; strcpy(f.tvMotivo, "recusou");
  resolverregra_testar(RES_S_NAO_COMECA, &f, &r);
  assert(r.primeira == RES_T_FONTE && r.acao == RES_ACAO_ENVIAR);
  assert(r.estado[RES_T_TV] == RES_NAO_TESTADO);

  // Cadeias por sintoma: Som = TV + audio; Legenda/Continuar nao testam nada.
  f = bons(); f.offline = 1;
  resolverregra_testar(RES_S_SOM, &f, &r);
  assert(r.primeira == -1 && !r.testado[RES_T_INTERNET] && r.testado[RES_T_TV] && r.testado[RES_T_AUDIO]);
  resolverregra_testar(RES_S_LEGENDA, &f, &r);
  assert(resolverregra_mascara(RES_S_CONTINUAR) == 0 && r.primeira == -1);
  resolverregra_testar(RES_S_CONTAS, &f, &r);
  assert(r.primeira == RES_T_INTERNET);
  resolverregra_testar(RES_S_TRAVA, &f, &r);
  assert(r.primeira == RES_T_INTERNET && !r.testado[RES_T_ADDONS]);

  // Ultimo erro: recente conta, com 7 h nao.
  resolverregra_falha_limpar();
  assert(!resolverregra_falha_recente(&fa, agora));
  resolverregra_falha_registrar(RES_T_FONTE, "A TV recusou o arquivo", "Tente outra", "T2:E5", agora);
  assert(resolverregra_falha_recente(&fa, agora + 240) && !strcmp(fa.midia, "T2:E5"));
  assert(!resolverregra_falha_recente(&fa, agora + 7 * 3600));
  resolverregra_falha_limpar();

  // Registros enviados: ordena, descarta 30 dias e lixo.
  snprintf(buf, sizeof buf, "AAAAAA %ld 0\nBBBBBB %ld 2\nvelho1 %ld 1\nlixo\nCCCCCC %ld 9\nXX %ld 0\n",
           agora - 100, agora - 50, agora - RES_GUARDA_S - 1, agora - 10, agora);
  n = resolverregra_enviados_ler(buf, agora, v, RES_ENVIADOS_MAX);
  assert(n == 3);
  assert(!strcmp(v[0].codigo, "CCCCCC") && v[0].sintoma == 0);   // sintoma fora da faixa -> 0
  assert(!strcmp(v[1].codigo, "BBBBBB") && !strcmp(v[2].codigo, "AAAAAA"));
  // Exatamente 30 dias ja saiu.
  snprintf(buf, sizeof buf, "AAAAAA %ld 0\n", agora - RES_GUARDA_S);
  assert(resolverregra_enviados_ler(buf, agora, v, RES_ENVIADOS_MAX) == 0);

  // add: topo, repetido substitui, teto respeitado; remover; ida e volta.
  n = 0;
  n = resolverregra_enviados_add(v, n, 3, "AAAAAA", agora - 30, 1);
  n = resolverregra_enviados_add(v, n, 3, "BBBBBB", agora - 20, 2);
  n = resolverregra_enviados_add(v, n, 3, "AAAAAA", agora - 10, 3);
  assert(n == 2 && !strcmp(v[0].codigo, "AAAAAA") && v[0].sintoma == 3);
  n = resolverregra_enviados_add(v, n, 3, "CCCCCC", agora, 0);
  n = resolverregra_enviados_add(v, n, 3, "DDDDDD", agora + 1, 0);
  assert(n == 3 && !strcmp(v[0].codigo, "DDDDDD") && !strcmp(v[2].codigo, "AAAAAA"));
  n = resolverregra_enviados_remover(v, n, "CCCCCC");
  assert(n == 2 && !strcmp(v[1].codigo, "AAAAAA"));
  h = resolverregra_enviados_serializar(v, n, buf, sizeof buf);
  assert(h > 0);
  assert(resolverregra_enviados_ler(buf, agora + 5, v, RES_ENVIADOS_MAX) == 2);
  assert(resolverregra_enviados_serializar(v, 2, buf, 8) == 0);   // nao coube

  // Codigo e tempo.
  assert(resolverregra_codigo_exibir("4F7K92", cod, sizeof cod) && !strcmp(cod, "NV-4F7K-92"));
  assert(!resolverregra_codigo_exibir("4F7", cod, sizeof cod) && !cod[0]);
  assert(resolverregra_ha(10, &n) == RES_HA_AGORA);
  assert(resolverregra_ha(240, &n) == RES_HA_MIN && n == 4);
  assert(resolverregra_ha(7300, &n) == RES_HA_H && n == 2);
  assert(resolverregra_ha(86400 * 3, &n) == RES_HA_DIAS && n == 3);

  puts("resolver: regra ok");
  return 0;
}
