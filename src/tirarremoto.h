#ifndef TIRARREMOTO_H
#define TIRARREMOTO_H
#include "trakt.h"
#include "syncprog.h"
#include <stdint.h>
// Capture before queueing; the original playback IDs and authentication are
// immutable even when another profile opens before the worker runs.
typedef struct {
  uint64_t sessao; int perfil; char usuario[80]; TraktRemocao trakt; SyncProgRemocao conta;
} TirarRemotoPedido;
int tirarremoto_capturar(TirarRemotoPedido *, const char *imdb);
void tirarremoto_executar_pedido(const TirarRemotoPedido *, const char *chave, int ocultar);
// Synchronous legacy entry. Only explicit removal hides future Trakt episodes;
// marking watched removes paused playback without hiding the series.
void tirarremoto_executar(const char *imdb, const char *chave, int ocultar);
#endif
