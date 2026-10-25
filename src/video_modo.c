// MODO DO LOAD DA LIVE TV (#158), comum a todos os alvos: so o video.c da
// webOS le (video.h). Fica num arquivo proprio para os outros backends nao
// precisarem de coto.
#include "video.h"
static int modoPedido;
void video_definir_modo_live(int modo) { modoPedido = modo >= 0 && modo <= 2 ? modo : 0; }
int  video_modo_live_consumir(void) { int m = modoPedido; modoPedido = 0; return m; }

// All four backends share the multiplier; unsupported or blocked speed is 1x.
static int taxaSync = 1000;
void video_taxa_sync(int m) { taxaSync = m >= 950 && m <= 1050 ? m : 1000; }
int video_taxa_efetiva(void) {
  if (!video_velocidade_suportada() || video_velocidade_bloqueada()) return 100;
  return (video_velocidade_atual() * taxaSync + 500) / 1000;
}
