#include "segurar.h"
#include "layout.h"
#include <stdio.h>

// Instante da ultima leitura de eventos (0 = segurar_quadro nunca chamado).
static Uint32 leitura;
// Pressao (pelo `desde`) que ja ganhou a linha de log.
static Uint32 logado;

void segurar_quadro(Uint32 agora) { leitura = agora; }

int segurar_venceu(Uint32 agora, Uint32 desde, const char *tela) {
  Uint32 ref = leitura ? leitura : agora;
  if (!desde) return 0;
  // KEYDOWN lido depois da ultima leitura registrada: ainda nao ha medida.
  if ((Sint32)(ref - desde) < (Sint32)NV_HOLD_MS) return 0;
  if (logado != desde) {
    logado = desde;
    printf("[segurar] %s: disparou com %u ms segurando (relogio da tela %u ms)\n",
           tela ? tela : "?", (unsigned)(ref - desde), (unsigned)(agora - desde));
    fflush(stdout);
  }
  return 1;
}
