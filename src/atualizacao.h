// Aviso de ATUALIZACAO — quando o GitHub tem uma release mais nova que a
// versao compilada, um cartao conta o que mudou, uma vez por versao.
#ifndef NV_ATUALIZACAO_H
#define NV_ATUALIZACAO_H
#include <SDL2/SDL.h>

// Dispara a consulta em segundo plano QUANDO A AGENDA MANDA: a primeira na
// primeira chamada, depois de novo a cada ~6 h (pelo relogio de parede, que
// anda com a TV dormindo). Chamar a cada quadro com a home de pe e sem player;
// fora da hora nao custa nada.
void atualizacao_verificar(void);
// A regra da agenda, pura (tests/atualizacao_agenda.c). rel/relUlt: time() de
// agora e do inicio da ultima consulta; tk/tkUlt: SDL_GetTicks idem;
// tkNaoAntes: nada antes disto (logo depois de voltar do segundo plano).
int  atualizacao_agenda_vence(long rel, long relUlt, Uint32 tk, Uint32 tkUlt,
                              Uint32 tkNaoAntes, int jaConsultou, int falhou);
// O app voltou do segundo plano (janela visivel/foco/foreground): a proxima
// consulta vencida espera alguns segundos, para nao disputar a rede com o
// resto do app acordando.
void atualizacao_retomou(void);
// "Procurar atualização" nos Ajustes: consulta AGORA, fora da agenda.
void atualizacao_procurar_agora(void);
// O que a ultima consulta respondeu, para a linha dos Ajustes.
enum { ATUALIZACAO_BUSCA_NADA = 0, ATUALIZACAO_BUSCA_PROCURANDO,
       ATUALIZACAO_BUSCA_EM_DIA, ATUALIZACAO_BUSCA_NOVA, ATUALIZACAO_BUSCA_ERRO };
int  atualizacao_busca(void);
// 1 (uma vez) quando a consulta pedida por atualizacao_procurar_agora achou
// versao nova: quem pediu abre o cartao.
int  atualizacao_busca_achou(void);
// Abre o cartao se a consulta achou versao nova ainda nao mostrada. Chamar
// quando a home esta de pe e nenhum outro cartao esta aberto.
void atualizacao_mostrar_se_houver(void);
int  atualizacao_aberta(void);
// ABRE O CARTAO A PEDIDO, ignorando a marca de "ja mostrado". E a porta de
// saida de quem apertou "Depois": o cartao so aparece sozinho UMA VEZ por
// versao, e sem isto aquele "Depois" valeria para sempre. Nao faz nada quando
// nao ha versao nova.
void atualizacao_abrir(void);
// Versao nova conhecida ("" se nenhuma) — para a linha de versao dos Ajustes.
const char *atualizacao_nova(void);
// 1 enquanto o cartao esta na tela (ou recolhendo): ele E a ilha crescida, e
// a pilula de baixo nao se desenha (app.c: ilha_coberta).
int  atualizacao_cobre_ilha(void);
void atualizacao_evento(const SDL_Event *e);
void atualizacao_atualizar(float dt, Uint32 agora);
void atualizacao_desenhar(Uint32 agora);
// Teste do ponteiro (#99): 0 = "Atualizar agora", 1 = "Depois".
int  atualizacao_teste_foco(void);

#endif
