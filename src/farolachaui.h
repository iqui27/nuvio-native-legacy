// TELA 7 DO FAROL — "Achados" (PLANO.md §2.9, §3.2 tela 7, §3.3 "Nada a achar",
// issue #25).
//
// Recebe uma intencao de 1 clique (FarolAchaTipo) ou um pedido parametrizado
// (`FarolAchaAbrir`, gancho para #31 ditado e #50 assistente P5), consulta
// `farolacha_listar_rev` / `farolacha_listar_genero`, exibe ate 3 cartazes com
// 2 linhas honestas acima (ou o estado "Nada a achar" + Voltar), e despacha
// a abertura pelo contrato de `busca_pediu_abrir` (id + tipo revalidados contra
// o catalogo vivo; nenhum ponteiro `const CatItem *` nem indice e guardado
// entre quadros).
//
// SEGURANCA DE cat_revisao():
//   Em todo acesso aos achados (desenho e abertura), valida via
//   `farolacha_item(&achados[i], rev)`. Se cat_revisao() mudou, refaz
//   `farolacha_listar_*` com a mesma `semente` antes de desenhar ou abrir.
#ifndef NV_FAROLACHAUI_H
#define NV_FAROLACHAUI_H

#include <SDL2/SDL.h>
#include "farolacha.h"

#define FAROL_ACHA_UI_MAX 3

// Pedido parametrizado para abrir a Tela 7 (gancho limpo para #31 e #50).
typedef struct {
  FarolAchaTipo tipo;
  int           maxMin;          // <= 0 usa FAROL_ACHA_CURTO_PADRAO
  const char   *genero;          // NULL ou "" = qualquer genero
  const char   *kicker;          // NULL = padrao ("Farol › Achar o que ver")
  const char   *l1Override;      // NULL = monta pela regra de honestidade
  const char   *l2Override;      // NULL = monta pela regra de honestidade
  unsigned      semente;         // 0 = gera semente automatica
} FarolAchaAbrir;

void farolachaui_reset(void);
void farolachaui_abrir(FarolAchaTipo tipo, int maxMin, unsigned semente);
void farolachaui_abrir_op(const FarolAchaAbrir *op);

// Processa evento de entrada (D-pad, OK, Voltar).
// `pediuVoltar`: 1 se a pessoa pediu Voltar (tecla Back ou OK no botao Voltar).
// `pediuAbrir`:  1 se a pessoa apertou OK num cartaz valido (id/tipo gravados
//                para `farolachaui_pediu_abrir` — o chamador deve fechar o Farol).
// Devolve 1 se consumiu o evento.
int  farolachaui_evento(const SDL_Event *e, int *pediuVoltar, int *pediuAbrir);
void farolachaui_atualizar(float dt, Uint32 agora);
void farolachaui_desenhar(float ox, float oy, float alpha, Uint32 agora);

// Mesmo contrato de `busca_pediu_abrir` (src/busca.h:27): devolve 1 UMA vez
// quando ha cartaz selecionado, resolvendo o indice vivo em `cat_item()` a
// partir de `id` e `tipo` e gravando em `*indiceCatalogo`.
int  farolachaui_pediu_abrir(int *indiceCatalogo);

// Estado observavel para testes e integracao com resolver.c / #31 / #50.
int           farolachaui_n(void);
int           farolachaui_foco(void);      // 0..n-1 = cartaz, -1 = botao Voltar
FarolAchaTipo farolachaui_tipo(void);
unsigned      farolachaui_revisao(void);
unsigned      farolachaui_semente(void);
int           farolachaui_em_fallback(void);
const char   *farolachaui_l1(void);
const char   *farolachaui_l2(void);

#endif
