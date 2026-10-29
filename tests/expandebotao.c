// O botao "Assistir do comeco" da pagina de detalhe (modo agrupado) abre a
// pilula e so depois mostra o rotulo. Regra do dono: o texto NAO pode entrar
// cortado pela largura em crescimento — "o texto ta meio que escrevendo na
// hora". Aqui e a regra do anim_expande (src/anim.h): o rotulo e 0 enquanto a
// largura nao chegou ao fim; ao fechar, o rotulo sai antes de a largura
// encolher. O desenho do rotulo INTEIRO (txt_linha, sem txt_linha_corta) e
// conferido pelo .sh no codigo da detail.c.
#include <assert.h>
#include <stdio.h>
#include "../src/anim.h"

// Um quadro de TV: 60 fps.
#define DT 0.016f

static void passo(float *larg, float *rot, float alvo, int n) {
  for (int i = 0; i < n; i++)
    anim_expande(larg, rot, alvo, DT, 160.0f, 120.0f);
}

int main(void) {
  // ABRIR: o rotulo e 0 enquanto a largura nao chegou ao fim.
  float larg = 0.0f, rot = 0.0f;
  int rotuloViuLarguraMenor = 0, larguraAntesDoRotulo = 0;
  for (int q = 0; q < 40 && (larg < 1.0f || rot < 1.0f); q++) {
    anim_expande(&larg, &rot, 1.0f, DT, 160.0f, 120.0f);
    if (rot > 0.0f) {
      if (larg < 1.0f) rotuloViuLarguraMenor = 1;
      if (!larguraAntesDoRotulo) larguraAntesDoRotulo = q + 1;
    }
  }
  assert(!rotuloViuLarguraMenor);            // rotulo 0 com largura < final
  assert(larg == 1.0f && rot == 1.0f);       // chegou aos dois
  // A largura termina antes do rotulo: ~160 ms (~10 quadros) contra 160+120.
  assert(larguraAntesDoRotulo >= 10 && larguraAntesDoRotulo <= 12);

  // FECHAR: o rotulo sai primeiro; a largura so encolhe com ele ja sumido.
  int larguraEncolheuComRotulo = 0, larguraPrimeiroQuadro = -1;
  for (int q = 0; q < 40 && (larg > 0.0f || rot > 0.0f); q++) {
    anim_expande(&larg, &rot, 0.0f, DT, 160.0f, 120.0f);
    if (rot > 0.0f && larg < 1.0f) larguraEncolheuComRotulo = 1;
    if (larg < 1.0f && larguraPrimeiroQuadro < 0) larguraPrimeiroQuadro = q + 1;
  }
  assert(!larguraEncolheuComRotulo);
  assert(larg == 0.0f && rot == 0.0f);
  assert(larguraPrimeiroQuadro >= 8);        // ~120 ms de rotulo antes da largura

  // INTERRUPCAO 1: fechar pela metade do rotulo e reabrir — a largura ja
  // estava no fim e CONTINUA nele; o rotulo volta a subir de onde parou.
  larg = 1.0f; rot = 1.0f;
  passo(&larg, &rot, 0.0f, 4);               // rotulo no meio do fade (~0.47)
  assert(larg == 1.0f && rot > 0.3f && rot < 1.0f);
  passo(&larg, &rot, 1.0f, 12);
  assert(larg == 1.0f && rot == 1.0f);

  // INTERRUPCAO 2: reabrir no meio da largura — o rotulo continua 0.
  larg = 0.5f; rot = 0.0f;
  passo(&larg, &rot, 0.0f, 3);
  assert(larg < 0.5f + 0.01f && larg > 0.0f && rot == 0.0f);
  passo(&larg, &rot, 1.0f, 20);
  assert(larg == 1.0f && rot == 1.0f);

  // MOVIMENTO REDUZIDO: direto ao alvo, sem interpolacao.
  anim_politica_reduzida = 1;
  larg = 0.0f; rot = 0.0f;
  anim_expande(&larg, &rot, 1.0f, DT, 160.0f, 120.0f);
  assert(larg == 1.0f && rot == 1.0f);
  anim_expande(&larg, &rot, 0.0f, DT, 160.0f, 120.0f);
  assert(larg == 0.0f && rot == 0.0f);
  anim_politica_reduzida = 0;

  puts("ok #205: expanding pill shows the label only at full width, unclipped");
  return 0;
}
