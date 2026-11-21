// Forma de pedido que o transporte WGT (fetch) de rede_pedir cumpre alem do
// GET publico em fluxo: GET com cabecalhos, redirects seguidos pelo proprio
// fetch, sem corpo de volta e com `parar` conferido na espera — e o que a
// sonda ativa e os diagnosticos pedem. O ramo so atende pedido marcado como
// sonda (p->sonda): sem a marca explicita um GET comum de buffer volta ao
// REDE_INDISPONIVEL de sempre, como no pai. Fica em cabecalho para o teste
// do predicado compilar no host, sem emscripten.h.
#ifndef NV_REDEPEDIDO_WGT_H
#define NV_REDEPEDIDO_WGT_H

#include <string.h>
#include "rede.h"

static inline int rede_pedido_wgt_sonda(const RedePedido *p) {
  return p && p->sonda && p->url && !p->receber_corpo && p->seguir &&
      (!strncmp(p->url, "https://", 8) || !strncmp(p->url, "http://", 7)) &&
      (!p->metodo || !strcmp(p->metodo, "GET")) &&
      !p->corpo && !p->n_corpo && !p->job && !p->intervalo &&
      !p->janela_corpo_ms && !p->ca_arquivo && !p->tls_pessoal &&
      !p->pino_publico && !p->mesmo_host && !p->rebaixar_midia &&
      p->prazo_ms <= 300000 && p->max_bytes <= REDE_CORPO_MAXIMO &&
      p->max_cabecalhos <= REDE_CAB_MAXIMO;
}

#endif
