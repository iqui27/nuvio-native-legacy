// O PADRAO DE MOVIMENTO DA ILHA, num lugar so.
//
// A ilha do relogio (ilha.c), a Central de Controle que nasce dela e o painel
// Social (salvospainel.c) andam na MESMA mola: subamortecida, com um repique
// curto so na forma ("o pulo da Dynamic Island"), nunca no texto. O dono
// (07/10): "a animacao da ilha tem de manter um padrao; o painel Social esta
// rapido demais e a troca de aba nao tem suavidade".
//
//   MOV_PILULA  w 10,0  zeta 0,72   a forma pequena e os indicadores (~0,45 s)
//   MOV_MODAL   w  7,5  zeta 0,80   a forma grande: modal, painel (~0,7 s)
//   MOV_CORPO   w 10,0  zeta 1,00   texto e conteudo: mesma velocidade, sem repique
//
// Animacoes reduzidas (ajuste ou politica de anim.h): vai direto ao alvo.
#ifndef NV_MOVIMENTO_H
#define NV_MOVIMENTO_H
#include <math.h>
#include "anim.h"
#include "ajustes.h"

#define MOV_PILULA_W 10.0f
#define MOV_PILULA_Z 0.72f
#define MOV_MODAL_W   7.5f
#define MOV_MODAL_Z  0.80f
#define MOV_CORPO_W  10.0f
#define MOV_CORPO_Z   1.00f

// Mola de 2a ordem (posicao x, velocidade *v), integrada em 4 subpassos.
static inline float mov_mola(float *v, float x, float alvo, float dt, float w, float z) {
  int k;
  if (anim_politica_reduzida || ajustes_animacoes_reduzidas()) { *v = 0.0f; return alvo; }
  if (dt > 0.05f) dt = 0.05f;
  for (k = 0; k < 4; k++) {
    float h = dt * 0.25f, ac = w * w * (alvo - x) - 2.0f * z * w * (*v);
    *v += ac * h;
    x += *v * h;
  }
  return x;
}
// Como mov_mola, mas assenta de vez (a cauda exponencial nunca chega la, e um
// painel nao pode ficar "animando" para sempre).
static inline float mov_mola_assenta(float *v, float x, float alvo, float dt, float w, float z) {
  x = mov_mola(v, x, alvo, dt, w, z);
  if (fabsf(alvo - x) < 0.0015f && fabsf(*v) < 0.02f) { *v = 0.0f; return alvo; }
  return x;
}
#endif
