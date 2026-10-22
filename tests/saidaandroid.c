// saida_android_nao_foi_queda: o que conta como queda para o aviso e o modo seguro.
#include <stdio.h>
#include "saidaandroid.h"

static int falhas;
static void caso(const char *s, int esperado) {
  int r = saida_android_nao_foi_queda(s);
  if (r != esperado) { printf("FALHOU: %s -> %d (esperado %d)\n", s ? s : "(nulo)", r, esperado); falhas++; }
}

int main(void) {
  caso("motivo=crash-nativo(5) status=11 importancia=100 pss=95MB rss=158MB ha=2s desc=crash", 0);
  caso("motivo=crash-java(4) status=0", 0);
  caso("motivo=ANR(6) status=0 desc=user request after error", 0);
  caso("motivo=pouca-memoria(3) status=0", 0);
  caso("motivo=sinal(2) status=9 importancia=400", 0);
  caso("motivo=desconhecido(16) status=0 desc=stop space.nuvio.nativelegacy due to installPackageLI", 1);
  caso("motivo=usuario(10) status=0 desc=[FORCE STOP] stop", 1);
  caso("motivo=usuario-parou(11) status=0", 1);
  caso("motivo=outro(13) status=0 desc=SwipeUpClean", 1);
  caso("motivo=permissao(8) status=0", 1);
  caso("motivo=desconhecido(15) status=0", 1);
  caso("", 0);
  caso(NULL, 0);
  caso("motivo=", 0);
  caso("motivo=x(", 0);
  caso("motivo=x(16", 0);
  caso("desc=motivo sem parenteses", 0);
  if (falhas) return 1;
  printf("saidaandroid: ok\n");
  return 0;
}
