// Colheita sintetica de PGS (S_HDMV/PGS) do MKV para o armazem imgsub.
// Transporte local de Range: nenhum socket, parser/extrator reais.
#include "mkvass.h"
#include "legenda.h"
#include "imgsub.h"
#include "dados.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mkvass_pgs_transport.h"
int main(int argc, char **argv) {
  assert(argc == 2); dados_iniciar(".");
  arquivo = argv[1];
  legenda_desligar(); imgsub_limpar();
  mkvass_aceitar_imagem(1);
  pthread_mutex_lock(&trava);
  mkvass_iniciar_ordinal(esperado, 0);
  if (strstr(arquivo, "-scan")) mkvass_folga(3.0);
  pthread_mutex_unlock(&trava);
  ImgsubQuadro *q = NULL;
  for (int i = 0; !q && i < 300; i++) { q = imgsub_pegar(1.5); if (!q) usleep(10000); }
  assert(q && q->n == 1);
  // The first frame can be published before the following clear is decoded.
  // Wait for the real reader completion, not a timing-dependent sleep.
  for (int i = 0; mkvass_estado() != MKVASS_COMPLETO && i < 3000; i++) usleep(1000);
  assert(mkvass_estado() == MKVASS_COMPLETO);
  assert(q->tela_w == 1920 && q->tela_h == 1080);
  assert(q->obj[0].x == 100 && q->obj[0].y == 900 && q->obj[0].w == 6 && q->obj[0].h == 2);
  assert(q->obj[0].rgba[0] == 255 && q->obj[0].rgba[3] == 255 && q->obj[0].rgba[7] == 128);
  assert(q->ini == 1.25 && q->fim == 3.0);
  imgsub_soltar(q);
  // Depois do fim do cue: nada. O bloco que so limpa devolve o quadro vazio
  // (a tela apaga mesmo se o cue anterior ainda estivesse na janela).
  assert(!imgsub_pegar(3.5));
  q = imgsub_pegar(5.5);
  assert(q && q->n == 0 && q->ini == 5.0);
  imgsub_soltar(q);
  assert(imgsub_dono_atual() == legenda_geracao());
  MkvassDiagnostico d;
  mkvass_diagnostico(&d);
  assert(d.packets == 2 && d.payload_bytes > 0);
  assert(d.decode_attempts == 2 && d.decoded == 2 && d.emitted == 2);
  assert(!d.restore_errors && !d.decode_errors && !d.pending_drops);
  ImgsubDiagnostico overlay;
  imgsub_diagnostico(&overlay);
  assert(overlay.queued == 2 && overlay.selected == 2);
  assert(overlay.drawn == 0); // Selection is not rendering.

  // Sem aceitar imagem a faixa nao e colhida (o app cai na rota anterior).
  parar(); imgsub_limpar(); legenda_desligar();
  mkvass_aceitar_imagem(0);
  mkvass_iniciar_ordinal(esperado, 0);
  usleep(500000);
  assert(!imgsub_pegar(1.5));
  parar();
  puts("mkvass_pgs: bloco PGS vira quadro no imgsub, limpeza e recusa ok");
}
