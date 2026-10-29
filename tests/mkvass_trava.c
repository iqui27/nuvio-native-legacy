// imgsub_limpar nunca roda sob S.trava: o armazem do imgsub tem trava propria e
// o fio de desenho a segura entre pegar/soltar. A trava e nao recursiva, entao
// o stub que reentra no mkvass trava se alguem limpar com S.trava tomada.
#include "mkvass.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int limpezas;
char *rede_baixar(const char *u, int s) { (void)u; (void)s; return NULL; }
char *rede_baixar_bin(const char *u, int s, long *n) { (void)u; (void)s; (void)n; return NULL; }
const char *rede_url_publica(const char *u, char *d, unsigned t) { (void)u; d[0] = 0; (void)t; return d; }
long rede_corte_host(const char *u) { (void)u; return 0; }
int rede_resto_recusado(void) { return 0; }
char *rede_baixar_trecho_st(const char *u, int s, long long i, long long f, long *t,
                            int *st, int *e, char *fin, unsigned tf) {
  (void)u; (void)s; (void)i; (void)f; (void)st; (void)e; (void)fin; (void)tf; *t = 0; return NULL;
}
void imgsub_por(unsigned d, double i, double f, PgsQuadro *q) { (void)d; (void)i; (void)f; (void)q; }
unsigned imgsub_dono_atual(void) { return 0; }
void imgsub_limpar(void) { limpezas++; (void)mkvass_estado(); }

static void trava(int s) { (void)s; static const char m[] = "FAIL: imgsub_limpar com S.trava tomada\n"; (void)!write(2, m, sizeof m - 1); _exit(1); }

int main(void) {
  dados_iniciar(".");
  signal(SIGALRM, trava); alarm(5);
  mkvass_parar();
  assert(limpezas == 1);
  mkvass_iniciar("https://example.test/a.mkv", 1);
  assert(limpezas == 2);
  mkvass_parar();
  for (int i = 0; mkvass_ocupado() && i < 500; i++) usleep(10000);
  puts("mkvass_trava: imgsub_limpar fora de S.trava ok");
  return 0;
}
