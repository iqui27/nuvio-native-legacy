// Stubs de rede/intro para testes que linkam src/capmkv.c (via video_android.c
// ou video_tpk.c) sem puxar rede.c/mkvass.c/intro.c. Sem socket: toda leitura
// lateral de capitulos falha ("sem capitulos"), que e o que esses testes querem.
#include "../src/intro.h"
#include "../src/mkvass.h"
#include "../src/rede.h"
void intro_definir_capitulos(const IntroTrecho *v, int n) { (void)v; (void)n; }
int mkvass_cabecalho(const char *url, unsigned char **buf, long *n) { (void)url; (void)buf; (void)n; return 0; }
char *rede_baixar_trecho_st(const char *url, int segundos, long long ini, long long fim,
                            long *tam, int *status, int *erro, char *final, unsigned tamFinal) {
  (void)url; (void)segundos; (void)ini; (void)fim; (void)final; (void)tamFinal;
  if (tam) *tam = 0;
  if (status) *status = 0;
  if (erro) *erro = 1;
  return 0;
}
// mkv_faixas_e_caps (mkv.c) baixa o cabecalho inteiro por aqui.
char *rede_baixar_trecho(const char *url, int segundos, long ini, long fim,
                         long *tam) {
  (void)url; (void)segundos; (void)ini; (void)fim;
  if (tam) *tam = 0;
  return 0;
}
