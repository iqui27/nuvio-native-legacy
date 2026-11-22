#include "asspeso.h"

void asspeso_zerar(AssPeso *p) {
  p->nivel = p->n = p->ruins = p->boas = 0;
  p->somaMs = p->somaBytes = 0.0;
}

int asspeso_divisor(const AssPeso *p) {
  return p->nivel <= 0 ? 1 : p->nivel == 1 ? 2 : 3;
}

int asspeso_sem_estilo(int modo) { return modo == ASSPESO_SIMPLES; }

static int trocar(AssPeso *p, int nivel) {
  p->nivel = nivel;
  p->n = p->ruins = p->boas = 0;
  p->somaMs = p->somaBytes = 0.0;
  return 1;
}

int asspeso_amostra(AssPeso *p, int modo, double ms, size_t bytes, int estouro) {
  double mediaMs, mediaBytes, volta;
  if (modo != ASSPESO_AUTO) {
    int mudou = p->nivel != 0;
    trocar(p, 0);
    return mudou;
  }
  if (estouro) return p->nivel < ASSPESO_NIVEL_MAX ? trocar(p, p->nivel + 1) : 0;
  if (ms < 0.0) ms = 0.0;
  p->somaMs += ms; p->somaBytes += (double)bytes;
  if (++p->n < ASSPESO_JANELA) return 0;
  mediaMs = p->somaMs / p->n; mediaBytes = p->somaBytes / p->n;
  p->n = 0; p->somaMs = p->somaBytes = 0.0;
  if (mediaMs > ASSPESO_RUIM_MS || mediaBytes > (double)ASSPESO_RUIM_BYTES) {
    p->boas = 0;
    if (++p->ruins >= ASSPESO_RUINS && p->nivel < ASSPESO_NIVEL_MAX) return trocar(p, p->nivel + 1);
    return 0;
  }
  p->ruins = 0;
  if (!p->nivel) return 0;
  /* Subir multiplica a area: 4x de metade para 1:1, 2,25x de um terco para
   * metade. So sobe se o custo PREVISTO la em cima couber em metade do limite,
   * para nao ficar indo e voltando. */
  volta = p->nivel == 1 ? 4.0 : 2.25;
  if (mediaMs * volta < ASSPESO_RUIM_MS * 0.5 && mediaBytes * volta < (double)ASSPESO_RUIM_BYTES * 0.5) {
    if (++p->boas >= ASSPESO_BOAS) return trocar(p, p->nivel - 1);
  } else p->boas = 0;
  return 0;
}
