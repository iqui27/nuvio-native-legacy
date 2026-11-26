// TELA AMBIENTE AO PAUSAR (NuvioMedia/NuvioTV#4142): com o painel de pausa de
// pe ha algum tempo, o quadro congelado do filme da lugar a ARTE DO TITULO em
// tela cheia, por baixo da ficha. O painel (pausao.c) continua dono do texto,
// das notas, do comentario e do elenco; aqui mora so a regra de QUANDO a arte
// entra e com que alfa. Pura (sem SDL nem GL) para o teste.
//
// O que ja foi decidido e nao se refaz aqui:
//   - a arte fica PARADA: a aproximacao lenta tremia na TV (descanso.c, dono,
//     05/10);
//   - uma textura so, a MESMA que o player ja pediu para a tela de carga
//     (tex_obter_hero(artehero_url)): pausado o decodificador segue ocupado, e
//     por isso a vitrine do descanso nao roda com o player aberto (main.c);
//   - a protecao de OLED continua com o esmaecer, que escurece por cima.
#ifndef NV_AMBIENTE_H
#define NV_AMBIENTE_H

#define AMB_ESPERA_MS  10000u   // do painel de pe ate a arte comecar a entrar
#define AMB_PEDIR_MS    6000u   // a textura e pedida antes, para estar pronta
#define AMB_ENTRA_MS    3000u   // duracao da entrada

typedef struct { unsigned t0; int entrou; } Ambiente;

// 1 = ja vale pedir a textura (`msPainel` = ha quanto o painel esta de pe).
int   ambiente_pedir(unsigned msPainel, int ligado, int temArte);

// Uma vez por quadro com o painel de pe. `pronta` = a textura ja esta na GPU.
// Devolve o alfa da arte (0..1). A entrada so comeca com a textura pronta: se
// ela chega tarde, a arte entra suave a partir dali, nunca aparece de estalo.
// Textura despejada no meio volta a 0 e entra de novo quando voltar.
float ambiente_passo(Ambiente *s, unsigned agora, unsigned msPainel,
                     int ligado, int pronta);

// O painel caiu: a proxima pausa recomeca do zero.
void  ambiente_zerar(Ambiente *s);

#endif
