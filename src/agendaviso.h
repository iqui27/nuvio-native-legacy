// O CARTAO DO LEMBRETE — o que "Lembrar-me" faz numa TV que nao tem push.
//
// Nem webOS nem Tizen entregam notificacao a um app fechado, e este app nao
// tem servico de fundo: nao existe como avisar alguem que nao esta com o Nuvio
// aberto. O que EXISTE e o mecanismo que o app ja usa para falar uma vez sobre
// uma coisa — o cartao de atualizacao (atualizacao.c) e o explicador do Social
// (recintro.c), os dois com marca por conteudo em disco.
//
// Este cartao segue a mesma disciplina: abre na home, uma unica vez por
// EPISODIO (a marca e a data do lembrete, gravada em lembretes-p<N>.txt), e
// lista o que estreou. Fechado, nao volta — a nao ser que o TMDB anuncie uma
// data nova, e ai e outro episodio.
#ifndef NV_AGENDAVISO_H
#define NV_AGENDAVISO_H
#include <SDL2/SDL.h>

// Abre se houver lembrete vencido ainda nao avisado. Chamar com a home de pe e
// nenhum outro cartao aberto, como atualizacao_mostrar_se_houver.
void agendaviso_mostrar_se_houver(void);
int  agendaviso_aberto(void);
void agendaviso_evento(const SDL_Event *e);
void agendaviso_atualizar(float dt, Uint32 agora);
void agendaviso_desenhar(Uint32 agora);

#endif
