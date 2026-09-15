// Aviso de ATUALIZACAO — quando o GitHub tem uma release mais nova que a
// versao compilada, um cartao conta o que mudou, uma vez por versao.
#ifndef NV_ATUALIZACAO_H
#define NV_ATUALIZACAO_H
#include <SDL2/SDL.h>

// Dispara a consulta em segundo plano. Chamar uma vez; repetir nao custa.
void atualizacao_verificar(void);
// Abre o cartao se a consulta achou versao nova ainda nao mostrada. Chamar
// quando a home esta de pe e nenhum outro cartao esta aberto.
void atualizacao_mostrar_se_houver(void);
int  atualizacao_aberta(void);
// Versao nova conhecida ("" se nenhuma) — para a linha de versao dos Ajustes.
const char *atualizacao_nova(void);
void atualizacao_evento(const SDL_Event *e);
void atualizacao_atualizar(float dt, Uint32 agora);
void atualizacao_desenhar(Uint32 agora);

#endif
