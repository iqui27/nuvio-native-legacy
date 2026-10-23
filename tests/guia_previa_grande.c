// GUIA TV v5: a opcao "Previa grande no Guia TV" (pedido do dono sobre o mock
// v5, "deixar o preview que nem da hoje"). Desligada — o PADRAO — vale o
// layout do mock: cartao de preview pequeno a direita do heroi (560x315,
// 16:9, o tamanho do cartao do preview-grade.png). Ligada, o preview volta ao
// tamanho do guia de hoje (800x450, a mesma classe de retangulo de
// integracao/2.0.5), e a ficha a esquerda REFLUI sem nunca sobrepor o video.
//
// Confere estado (padrao desligado) e retangulo (guia_preview_rect, o mesmo
// numero que posiciona o plano de video do player) nos dois modos.
#include "badges.h"
#include "../src/guia.c"
#include <assert.h>
#include <stdio.h>

int main(void) {
  float x, y, w, h, smallTop;
  // Padrao de fabrica: DESLIGADO (o layout do mock v5 manda).
  assert(!ajustes_guia_previa_grande());
  guia_preview_rect(&x, &y, &w, &h);
  assert(w == 560.0f && h == 315.0f);                    // cartao do mock, 16:9
  smallTop = G_TOPO;
  assert(y == 175.5f && G_PREVIEW_RAIO == 16.0f);
  assert(x + w <= NV_TELA_W - NV_MARGEM_X + 0.01f);      // encostado na margem
  assert(G_INFO_X + G_INFO_W + 48.0f <= x + 0.01f);      // ficha nao encosta
  // Ligado: o preview grande de hoje, 800x450, no topo do heroi
  // (y 108..558), encostado na margem direita — o layout da 2.0.5/2.0.8.
  ajustes_definir_guia_previa_grande(1);
  assert(ajustes_guia_previa_grande());
  guia_preview_rect(&x, &y, &w, &h);
  assert(w == 800.0f && h == 450.0f);
  assert(x == NV_TELA_W - NV_MARGEM_X - w && y == 108.0f);  // margem, y 108
  assert(G_INFO_X + G_INFO_W + 48.0f <= x + 0.01f);         // 48 de respiro
  assert(G_INFO_W == 912.0f);                               // ficha x 80..992
  assert(G_TOPO == 580.0f && G_TOPO > smallTop);
  assert(G_PREVIEW_RAIO == 0.0f);
  // De volta ao padrao: o cartao pequeno retorna.
  ajustes_definir_guia_previa_grande(0);
  guia_preview_rect(&x, &y, &w, &h);
  assert(w == 560.0f && h == 315.0f);
  assert(G_TOPO == smallTop && y == 175.5f);
  printf("guia_previa_grande: ok\n");
  return 0;
}
