// FOCO COMUM DOS CARTOES (Home e Detalhe): a mesma regra, os mesmos Ajustes.
//
//  - Nenhum anel na TV 2.1: o foco do cartaz e sombra + crescimento
//    (FOCO_ZOOM). A opcao antiga fica gravada so por compatibilidade.
//  - Profundidade (AJ_PROF + AJ_PROF_BRILHO): o reflexo que passa e o clarear
//    do cartao em foco, no proprio GFX_CARD (gfx.c), em todo tipo de cartao.
// `f` e a animacao de foco 0..1 que cada tela ja tem; nada aqui guarda estado.
#ifndef NV_FOCOPROF_H
#define NV_FOCOPROF_H
#include "gfx.h"
#include "ajustes.h"
#include "layout.h"
#include "ponteiro.h"

#define FOCO_ZOOM 0.06f   // o mesmo 6 % da Home com o anel desligado

// Caixa do cartao em foco: cresce em volta do centro; a elevacao e o foco.
static inline GfxRect foco_zoom(GfxRect r, float f) {
  float s;
  if (f <= 0.0f) return r;
  s = (layout_desktop_ativo() ? .025f : FOCO_ZOOM) * f;
  return (GfxRect){ r.x - r.w * s * 0.5f, r.y - r.h * s * 0.5f,
                    r.w * (1.0f + s), r.h * (1.0f + s) };
}

// Anel de foco. `raio` na convencao do gfx_cor (fracao do MENOR lado).
static inline void foco_anel(GfxRect r, float raio, float f, float a) {
  float menor = r.w < r.h ? r.w : r.h, ar, ag, ab;
  if (layout_desktop_ativo()) {
    if (f > .01f && !ponteiro_mouse()) {
      ajustes_acento(&ar,&ag,&ab);
      gfx_anel(r,raio,2.0f / gfx_escala(),ar,ag,ab,.4f*f*a);
    }
    return;
  }
  if (f <= 0.01f) return;
  // 2.1: cartaz em foco sobe por sombra + escala, sem aro — nem no acento nem
  // branco. A opcao antiga "Borda no cartaz em foco" permanece gravada para
  // compatibilidade, mas nao reintroduz contorno na linguagem nova.
  (void)raio; (void)menor; (void)ar; (void)ag; (void)ab;
  gfx_vidro_elevacao(r, f, a);
}

#endif
