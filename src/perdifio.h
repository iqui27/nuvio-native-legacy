// "O que eu perdi?": o pedido ao Gemini, fora do fio da interface.
// Um pedido por vez; um pedido novo (ou cancelar) descarta o anterior.
// A chave so vive na memoria do fio e no cabecalho x-goog-api-key do POST
// para PERDI_URL (Google). Nada aqui escreve log.
#ifndef NV_PERDIFIO_H
#define NV_PERDIFIO_H
#include "perdi.h"

typedef enum { PERDIFIO_NADA, PERDIFIO_CARREGANDO, PERDIFIO_PRONTO } PerdiFioEstado;

// Dispara. `corpo` (de perdi_pedido) passa a ser do modulo, que o libera.
// Devolve o numero do pedido (0 = nao disparou).
unsigned perdifio_pedir(const char *chave, char *corpo);
// PRONTO preenche *res e, se PERDI_OK, dst. Outro `id` que o corrente = NADA.
PerdiFioEstado perdifio_estado(unsigned id, PerdiResultado *res, char *dst, size_t tam);
// Descarta o pedido corrente: o fio dele aborta a transferencia (rede_parar_quando)
// e e juntado num perdifio_pedir/perdifio_cancelar seguinte.
void perdifio_cancelar(void);
// Saida do app: cancela tudo e junta todos os fios (pthread_join). Depois disso
// nenhum pedido novo dispara.
void perdifio_encerrar(void);

#endif
