#include "farolsonda.h"
#include "rede.h"
#include "redepedido_wgt.h"
#include <assert.h>
#include <string.h>
#include <unistd.h>

int player_aberto(void) { return 0; }
int player_mini_ativo(void) { return 0; }
int rede_pedir(const RedePedido *p, RedeResposta *r) {
  /* O transporte WGT de rede_pedir (src/rede.c) so atende esta forma; o
   * predicado de producao e conferido aqui, nao uma copia dele. */
  assert(rede_pedido_wgt_sonda(p));
  assert(p->prazo_ms <= 2000);
  assert(p->max_bytes == 2 || p->max_bytes == 512);
  if (p->max_bytes == 2) {
    assert(p->cabecalhos && !strcmp(p->cabecalhos[0], "Range: bytes=0-1"));
  }
  memset(r, 0, sizeof *r);
  r->erro = REDE_INDISPONIVEL;
  return 0;
}
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
int main(void) {
  FarolSonda s;
  assert(farolsonda_iniciar("https://synthetic.invalid/", NULL));
  for (int i = 0; i < 1000; i++) {
    if (farolsonda_estado(&s)) {
      assert(s.http == 0 && s.range == -1 && s.ttfb_ms == -1);
      assert(!s.cancelada);
      return 0;
    }
    usleep(1000);
  }
  assert(0);
}
