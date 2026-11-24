// FAROLCONSENTUI — tela de consentimento "Vai junto / Nunca vai" do gosto
// (issue 53, PLANO-GOSTO.md §5.3 e §6 #53).
//
// Mesmo padrao visual de duas colunas de farolenvioui.c (grade 1920x1080,
// vidro Glass 2.1, resolver_escala(), ajustes_animacoes_reduzidas(), ponto
// de luz de 10 px no acento antes do titulo). Colunas preenchidas diretamente
// por farolconsent_vai_junto() e farolconsent_nunca_vai().
#ifndef NV_FAROLCONSENTUI_H
#define NV_FAROLCONSENTUI_H

#include <SDL2/SDL.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  FCUI_B_SO_TV = 300,
  FCUI_B_PERMITIR
};

int  farolconsentui_presente(void);
void farolconsentui_abrir(void);
void farolconsentui_fechar(void);
int  farolconsentui_aberto(void);
void farolconsentui_esquecer(void);
void farolconsentui_evento(const SDL_Event *e);
void farolconsentui_atualizar(float dt);
void farolconsentui_desenhar(float ox, float oy, float a);

// Inspecao / testes
int  farolconsentui_foco(void);

#ifdef __cplusplus
}
#endif

#endif // NV_FAROLCONSENTUI_H
