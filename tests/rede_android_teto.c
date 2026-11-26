/* Teto de bytes na reserva HTTP do Android (viaAndroid / android_http_teto).
 * Sem rede e sem aparelho: o double faz o papel do NuvioActivity.httpPedirTeto
 * (le no maximo teto + 1 bytes) e o rede.c tem de cortar no teto exato, igual
 * ao recebedor da libcurl — e o kit de Home (homekitapp.c) passa a ser recusado
 * pelo tamanho, em vez de a TV encher a memoria com um corpo sem fim.
 *   bash tests/rede_android_teto.sh */
#define NV_ANDROID 1
#include <assert.h>
#include <stdarg.h>
#include "../src/rede.c"
#include "homekit.h"

static long servidor;     /* bytes que o servidor manda */
static int javaCapa = 1;  /* 0 = host sem teto na leitura (como o nv_host_http do iOS) */
static long tetoVisto = -1, chamadasSemTeto;

char *android_http_teto(const char *v, const char *u, const char *c, const char *b, int p,
                        long teto, int *st, int *retry, long *n, char *e, size_t ne) {
  long k = servidor;
  char *r;
  (void)v; (void)u; (void)c; (void)b; (void)p; (void)e; (void)ne;
  tetoVisto = teto;
  if (javaCapa && teto > 0 && k > teto + 1) k = teto + 1;   /* httpPedirTeto/lerCorpo */
  r = (char *)malloc((size_t)k + 1);
  assert(r);
  memset(r, ' ', (size_t)k);
  if (k) r[0] = '{';
  r[k] = 0;
  *st = 200; if (retry) *retry = 0; *n = k;
  return r;
}
char *android_http(const char *v, const char *u, const char *c, const char *b, int p,
                   int *st, int *retry, long *n, char *e, size_t ne) {
  chamadasSemTeto++;
  return android_http_teto(v, u, c, b, p, 0, st, retry, n, e, ne);
}
char *android_http_seguro(const char *v, const char *u, const char *c, const char *b, int p,
                          int *st, int *retry, long *n, char *e, size_t ne) {
  return android_http(v, u, c, b, p, st, retry, n, e, ne);
}
void android_etapa(const char *s) { (void)s; }
void android_quadro(void) {}

static char *baixar(long teto, RedeMedida *m) {
  RedeControle c = { teto, NULL };
  memset(m, 0, sizeof *m);
  tetoVisto = -1;
  return rede_baixar_medido_controle("https://kit.example/k.json", 1, NULL,
                                     teto > 0 ? &c : NULL, m);
}

int main(void) {
  RedeMedida m;
  char *r;
  HkKit kit;
  curlRuim = 1;   /* libcurl fora: todo pedido simples vai pela reserva do Android */

  /* Corpo maior que o teto: corta no teto exato e conta como limitado. */
  servidor = 5000;
  r = baixar(1000, &m);
  assert(r && tetoVisto == 1000);
  assert((long)strlen(r) == 1000 && m.bytes == 1000 && m.limitado == 1 && m.status == 200);
  free(r);

  /* Host que nao para de ler (iOS): o corte do rede.c vale igual. */
  javaCapa = 0;
  r = baixar(1000, &m);
  assert(r && (long)strlen(r) == 1000 && m.bytes == 1000 && m.limitado == 1);
  free(r);
  javaCapa = 1;

  /* Exatamente no teto e abaixo dele: inteiro, sem marca de limitado. */
  servidor = 1000;
  r = baixar(1000, &m);
  assert(r && (long)strlen(r) == 1000 && m.bytes == 1000 && m.limitado == 0);
  free(r);
  servidor = 10;
  r = baixar(1000, &m);
  assert(r && (long)strlen(r) == 10 && m.bytes == 10 && m.limitado == 0);
  free(r);

  /* Sem teto: os outros chamadores continuam recebendo o corpo inteiro. */
  servidor = 300000;
  r = baixar(0, &m);
  assert(r && tetoVisto == 0 && (long)strlen(r) == 300000 && m.limitado == 0);
  free(r);
  /* O teto e do pedido: nao vaza para o proximo do mesmo fio. */
  assert(redeLimiteAtual() == 0);

  /* Kit de Home (homekitapp.c baixarFio): teto HK_CORPO_MAX + 1. Um servidor
   * que manda 1 MB chega cortado em HK_CORPO_MAX + 1, e o hk_ler recusa. */
  servidor = 1024L * 1024;
  r = baixar((long)HK_CORPO_MAX + 1, &m);
  assert(r && tetoVisto == (long)HK_CORPO_MAX + 1);
  assert((long)strlen(r) == (long)HK_CORPO_MAX + 1 && m.limitado == 1);
  memset(&kit, 0, sizeof kit);
  assert(hk_ler(r, &kit, NULL) == HK_ERR_GRANDE);
  free(r);

  puts("rede_android_teto: ok");
  return 0;
}
