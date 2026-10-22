// Vigia de trava do canal ao vivo (src/livestall.h).
#include <assert.h>
#include <stdio.h>
#include "../src/livestall.h"

int main(void) {
  NvLiveStall s;
  unsigned t = 1000;
  int r;
  double p = 0;

  // Andando: nada acontece.
  nv_ls_zerar(&s);
  for (; t < 21000; t += 500) { p += 0.5; assert(nv_ls_passo(&s, t, p, 1, 0, 0, 0) == NV_LS_NADA); }

  // Posicao parada 8 s depois de andar: agenda a reabertura (espera 1 s).
  for (r = NV_LS_NADA; t < 40000 && r == NV_LS_NADA; t += 500) r = nv_ls_passo(&s, t, p, 1, 0, 0, 0);
  assert(r == NV_LS_AGENDOU && s.tentativa == 1);
  assert(t - 500 - 20500 >= NV_LS_PARADO_MS && t - 500 - 20500 < NV_LS_PARADO_MS + 1000);
  assert(nv_ls_passo(&s, t, p, 1, 0, 0, 0) == NV_LS_ESPERA);
  t += 1000;
  assert(nv_ls_passo(&s, t, p, 1, 0, 0, 0) == NV_LS_REABRIR);
  nv_ls_reaberto(&s, t);
  // Abrindo (posicao 0, vigia desarmado): nao reabre de novo por estar parado.
  for (r = 0; r < 100; r++) { t += 500; assert(nv_ls_passo(&s, t, 0, 0, 0, 0, 0) == NV_LS_NADA); }
  assert(nv_ls_passo(&s, t, 0, 1, 0, 0, 0) == NV_LS_NADA);  // pronto mas sem andar ainda

  // Fim de fluxo (o caso do log 45605): reabre na hora do evento, sem esperar os 8 s.
  nv_ls_zerar(&s);
  t = 5000; p = 0;
  for (r = 0; r < 10; r++) { t += 500; p += 0.5; nv_ls_passo(&s, t, p, 1, 0, 0, 0); }
  assert(nv_ls_passo(&s, t + 500, p, 1, 0, 1, 0) == NV_LS_AGENDOU);

  // Pausa da pessoa nunca conta, por mais que dure.
  nv_ls_zerar(&s);
  t = 1000; p = 3.0; nv_ls_passo(&s, t, p, 1, 0, 0, 0); nv_ls_passo(&s, t + 500, p + 1, 1, 0, 0, 0);
  for (r = 0; r < 200; r++) { t += 1000; assert(nv_ls_passo(&s, t, p, 1, 1, 1, 60000) == NV_LS_NADA); }
  // ...e ao despausar o relogio de trava recomeca.
  assert(nv_ls_passo(&s, t + 100, p, 1, 0, 0, 0) == NV_LS_NADA);

  // Bufferando 8 s depois de ter andado: reabre. Antes de andar: nao (abertura).
  nv_ls_zerar(&s);
  assert(nv_ls_passo(&s, 1000, 0, 1, 0, 0, 9000) == NV_LS_NADA);
  nv_ls_passo(&s, 2000, 1.0, 1, 0, 0, 0);
  assert(nv_ls_passo(&s, 2500, 1.0, 1, 0, 0, 8000) == NV_LS_AGENDOU);

  // Esgota: NV_LS_MAX reaberturas sem andar 20 s, depois DESISTIR (proxima fonte).
  nv_ls_zerar(&s);
  t = 1000; p = 1.0;
  nv_ls_passo(&s, t, p, 1, 0, 0, 0); t += 100; nv_ls_passo(&s, t, p + 1, 1, 0, 0, 0); p += 1;
  for (r = 1; r <= NV_LS_MAX; r++) {
    assert(nv_ls_passo(&s, t, p, 1, 0, 1, 0) == NV_LS_AGENDOU && s.tentativa == r);
    t += nv_ls_espera_ms(r);
    assert(nv_ls_passo(&s, t, p, 1, 0, 1, 0) == NV_LS_REABRIR);
    nv_ls_reaberto(&s, t);
    // anda 1 s e morre de novo (nao chega aos 20 s firmes)
    t += 200; p = 0.5; nv_ls_passo(&s, t, p, 1, 0, 0, 0); t += 500; p += 0.5; nv_ls_passo(&s, t, p, 1, 0, 0, 0);
  }
  assert(nv_ls_passo(&s, t, p, 1, 0, 1, 0) == NV_LS_DESISTIR);

  // Provedor que corta a cada ~30 s: andar 20 s firmes zera o contador, reconecta para sempre.
  nv_ls_zerar(&s);
  t = 1000; p = 0;
  for (int ciclo = 0; ciclo < 20; ciclo++) {
    int j;
    p = 0;
    for (j = 0; j < 60; j++) { t += 500; p += 0.5; assert(nv_ls_passo(&s, t, p, 1, 0, 0, 0) == NV_LS_NADA); }
    assert(nv_ls_passo(&s, t + 100, p, 1, 0, 1, 0) == NV_LS_AGENDOU);
    assert(s.tentativa == 1);
    t += 1200;
    assert(nv_ls_passo(&s, t, p, 1, 0, 1, 0) == NV_LS_REABRIR);
    nv_ls_reaberto(&s, t);
  }
  puts("livestall: ok");
  return 0;
}
