#include <assert.h>
#include <stdio.h>
#include "../src/descdebounce.h"
int main(void) {
  NvDescDeb d = {0, 0, 0};
  unsigned long long t;
  int iniciadas = 0, coalescidas = 0;
  // primeira volta do processo: imediata
  assert(nv_desc_pedido(&d, 1000) == 0);
  nv_desc_iniciou(&d, 1000); iniciadas++;
  // volta recente nao pode ser condenada; passado o minimo, pode
  assert(!nv_desc_pode_condenar(&d, 1000 + 3000));
  assert(nv_desc_pode_condenar(&d, 1000 + NV_DESC_MIN_MS));
  // pedido cedo: agenda o que falta; o seguinte e coalescido
  assert(nv_desc_pedido(&d, 1000 + 4000) == (long)(NV_DESC_MIN_MS - 4000));
  assert(nv_desc_pedido(&d, 1000 + 5000) == -1);
  nv_desc_iniciou(&d, 1000 + NV_DESC_MIN_MS); iniciadas++;
  assert(!d.adiado);

  // TEMPESTADE: um pedido por segundo durante 2 min. Antes: ~1 volta por
  // pedido (29 medidas). Agora: no maximo 1 a cada 10 s.
  NvDescDeb e = {0, 0, 0};
  unsigned long long adiadoPara = 0;
  iniciadas = 0;
  for (t = 0; t <= 120000; t += 1000) {
    if (adiadoPara && t >= adiadoPara) { nv_desc_iniciou(&e, t); iniciadas++; adiadoPara = 0; }
    long w = nv_desc_pedido(&e, t);
    if (w == 0) { nv_desc_iniciou(&e, t); iniciadas++; }
    else if (w > 0) adiadoPara = t + (unsigned long long)w;
    else coalescidas++;
  }
  printf("tempestade: %d voltas em 120 s, %d pedidos coalescidos\n", iniciadas, coalescidas);
  assert(iniciadas <= 13 && coalescidas > 50);
  // e o ultimo pedido nunca se perde: sempre ha volta depois dele
  assert(iniciadas >= 12);
  puts("descdebounce ok");
  return 0;
}
