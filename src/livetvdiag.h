// DIAGNOSTICO DA LIVE TV: tela propria (TELA_LIVETV_DIAG), aberta pelo VERDE
// no guia ou por Ajustes > Conteudo > Live TV. Mede a rede ate o provedor, le
// a conta Xtream e testa os canais da fileira em foco no guia — se tocam, em
// que formato, com que resolucao e codec e em quanto tempo — e sugere ajustes
// da Live TV (resolucao principal, formato do Xtream, espera), com um botao
// para aplica-los. Tudo sai tambem como linhas [livetv-diag] no registro.
#ifndef NV_LIVETVDIAG_H
#define NV_LIVETVDIAG_H
#include <SDL2/SDL.h>

void livetvdiag_iniciar(void);
void livetvdiag_evento(const SDL_Event *e);
void livetvdiag_atualizar(float dt, Uint32 agora);
void livetvdiag_desenhar(Uint32 agora);
int  livetvdiag_quer_sair(void);
void livetvdiag_encerrar(void);
// Testes do ponteiro (#99): o botao do rodape em foco (L.botao).
int  livetvdiag_teste_foco(void);

#endif
