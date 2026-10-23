// PRESS-DIP DO OK: o card afunda enquanto a tecla esta apertada e volta quando
// ela sobe. O feedback vem no KEYDOWN; a acao continua no KEYUP (home.c ja
// decide ali, porque so la se sabe se foi toque ou segurar — NV_HOLD_MS).
//
// A ideia e a do press.rs do plx-native (DIP = 0.918): o toque ganha resposta
// imediata sem mover a ativacao. Reimplementada na nossa mola, sem codigo deles.
#ifndef NV_PRESSDIP_H
#define NV_PRESSDIP_H
#include "anim.h"

#define PRESSDIP_ESCALA 0.918f
// ~14 ms de constante de tempo: afunda em ~2 quadros, volta no mesmo ritmo.
#define PRESSDIP_RIGIDEZ 70.0f

// Escala multiplicadora do card focado no proximo quadro (1 = em repouso).
static inline float pressdip_passo(float atual, int apertado, float dt) {
  return anim_mola(atual, apertado ? PRESSDIP_ESCALA : 1.0f, dt, PRESSDIP_RIGIDEZ);
}
#endif
