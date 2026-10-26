// RELOGIO DO QUADRO, com zerada na suspensao (row 30 do mapa de erros).
//
// O medidor de quadro media o tempo entre dois quadros pelo relogio de alta
// resolucao. Com o app suspenso o laco PARA no meio e o relogio continua: o
// primeiro quadro de volta entregava a suspensao inteira como se fosse UM
// quadro — 4191 s, 881 s, 54 s, 30 s em seis registros — e a triagem passava a
// procurar jank que nunca existiu. O clamp de 100 ms segurava a ANIMACAO, mas
// `pior=` e `swap=` (medicao) vazavam.
//
// Quem suspende ou volta marca `zerar`; o proximo quadro zera os dois relogios
// uma vez e nao conta a suspensao. Sem marca, o comportamento e o de sempre.
#ifndef NV_QUADROC_H
#define NV_QUADROC_H

#include <stdint.h>

typedef struct {
  uint64_t quadro;   // relogio de alta resolucao do ultimo quadro (0 = nenhum)
  uint32_t relato;   // SDL_GetTicks do ultimo relatorio de 3 s
  int      zerar;    // 1 = o proximo quadro nao mede (suspendeu/voltou)
  double   ultimo;   // dt do ultimo quadro medido, ms (para o teste)
} QuadroRelogio;

// Suspendeu ou voltou do segundo plano: o proximo quadro nao mede.
static inline void quadroc_suspendeu(QuadroRelogio *q) { q->zerar = 1; }

// Marca o quadro e devolve o dt em ms. 0 quando a suspensao pediu a zerada.
static inline double quadroc_quadro(QuadroRelogio *q, uint64_t agora, double porMs,
                                    uint32_t ticks) {
  double dt;
  if (q->zerar) {
    q->zerar = 0;
    q->quadro = agora;
    q->relato = ticks;
    q->ultimo = 0.0;
    return 0.0;
  }
  if (!q->quadro) { q->quadro = agora; q->relato = ticks; q->ultimo = 0.0; return 0.0; }
  dt = (double)(agora - q->quadro) * 1000.0 / porMs;
  q->quadro = agora;
  if (dt < 0.0) dt = 0.0;
  q->ultimo = dt;
  return dt;
}

#endif
