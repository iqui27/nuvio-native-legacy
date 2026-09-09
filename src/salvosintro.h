// EXPLICADOR DE PRIMEIRA VEZ do "Salvos", mostrado uma unica vez depois desta
// atualizacao.
//
// Por que ele existe: a tecla AZUL mudou de dono. Quem ja usava o app abria o
// painel "Sua atividade" com ela; a partir de agora abre "Salvos". Trocar o
// destino de um atalho sem avisar e a maneira mais rapida de a pessoa achar que
// o app quebrou — ela aperta, ve outra coisa, e nao tem como descobrir para
// onde foi o que existia antes.
//
// Ele tambem faz a UNICA pergunta que o app nao consegue responder sozinho:
// para onde o "+" deve escrever. Ver a nota em salvosintro.c — as duas opcoes
// nao sao equivalentes neste app, e o texto na tela diz isso.
//
// Segue o padrao de registro_aviso_primeira_vez (registro.c): a marca so e
// gravada DEPOIS de o cartao ter sido visto e respondido. Se o app morrer com
// ele na tela, ele volta — que e o certo.
#ifndef NV_SALVOSINTRO_H
#define NV_SALVOSINTRO_H
#include <SDL2/SDL.h>

// Decide se o explicador deve aparecer, lendo a marca em disco. Chamar quando o
// app ja esta na home e ja tem pasta de dados; e barata e idempotente (decide
// uma vez por processo).
void sintro_primeira_vez(void);

int  sintro_aberto(void);
void sintro_evento(const SDL_Event *e);
void sintro_atualizar(float dt, Uint32 agora);
void sintro_desenhar(Uint32 agora);

#endif
