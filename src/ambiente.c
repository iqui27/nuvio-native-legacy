// Ver ambiente.h.
#include "ambiente.h"

int ambiente_pedir(unsigned msPainel, int ligado, int temArte) {
  return ligado && temArte && msPainel >= AMB_PEDIR_MS;
}

void ambiente_zerar(Ambiente *s) { s->t0 = 0; s->entrou = 0; }

float ambiente_passo(Ambiente *s, unsigned agora, unsigned msPainel,
                     int ligado, int pronta) {
  float u;
  if (!ligado || !pronta || msPainel < AMB_ESPERA_MS) { ambiente_zerar(s); return 0.0f; }
  if (!s->entrou) { s->entrou = 1; s->t0 = agora; }
  u = (float)(agora - s->t0) / (float)AMB_ENTRA_MS;
  if (u >= 1.0f) return 1.0f;
  return u * u * (3.0f - 2.0f * u);
}
