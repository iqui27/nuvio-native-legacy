/* Predicado WGT de rede_pedir compilado no host: aceita a forma que a sonda
 * ativa pede (GET com cabecalhos, redirect e parar, sem corpo de volta) e
 * recusa o que o fetch nao cumpre. */
#include "redepedido_wgt.h"
#include <assert.h>
#include <string.h>

static int parar(void *u) { (void)u; return 0; }
static int corpo(const void *b, size_t n, void *u) {
  (void)b; (void)n; (void)u; return 1;
}

int main(void) {
  const char *cab[] = {"Range: bytes=0-1", "X-Probe: synthetic", NULL};
  RedePedido p = {0};

  /* Sonda da fonte: GET https com cabecalhos, seguir, parar, teto,
   * marcada explicitamente como sonda. */
  p.url = "https://fonte.invalid/video.mkv";
  p.cabecalhos = cab;
  p.seguir = 1;
  p.sonda = 1;
  p.parar = parar;
  p.max_bytes = 2;
  p.max_cabecalhos = 8192;
  p.prazo_ms = 2000;
  assert(rede_pedido_wgt_sonda(&p));

  /* O mesmo formato sem a marca de sonda e um GET comum: fica fora do
   * transporte fetch (REDE_INDISPONIVEL, como no pai). */
  { RedePedido q = p; q.sonda = 0; assert(!rede_pedido_wgt_sonda(&q)); }

  /* Sonda do Worker: mesmo formato, sem cabecalhos, http simples. */
  p.cabecalhos = NULL;
  p.url = "http://worker.invalid/v1/saude";
  p.max_bytes = 512;
  assert(rede_pedido_wgt_sonda(&p));

  /* O que o fetch nao cumpre continua fora. */
  { RedePedido q = p; q.metodo = "POST"; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.seguir = 0; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.receber_corpo = corpo; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.job = (RedeJob *)p.parar; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.janela_corpo_ms = 1000; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.tls_pessoal = 1; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.pino_publico = "sha256//AAAA"; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.mesmo_host = 1; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.rebaixar_midia = 1; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.corpo = "x"; q.n_corpo = 1; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.url = "ftp://fonte.invalid/"; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.prazo_ms = 300001; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.max_bytes = REDE_CORPO_MAXIMO + 1; assert(!rede_pedido_wgt_sonda(&q)); }
  { RedePedido q = p; q.max_cabecalhos = REDE_CAB_MAXIMO + 1; assert(!rede_pedido_wgt_sonda(&q)); }
  assert(!rede_pedido_wgt_sonda(NULL));
  return 0;
}
