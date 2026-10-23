// "O QUE ACHOU?" — a reacao nos creditos (Tela D do redesenho do Social).
//
// QUANDO PERGUNTA (reacao_regra_perguntar, pura, testada):
//   FILME: no mesmo instante em que o pos-reproducao sobe "Mais como este"
//          (posplay_regra_filme: marcador de creditos aceito, senao a
//          estimativa proporcional; nunca antes da metade).
//   SERIE: so no FIM DA TEMPORADA ou no ULTIMO EPISODIO DISPONIVEL, na janela
//          do cartao de proximo episodio (player_regra_proximo). Episodio do
//          meio da temporada NAO pergunta: o cartao "A seguir" e o convite que
//          importa ali, e perguntar a cada episodio seria cobrar a mesma
//          resposta dez vezes. A reacao e por TITULO (a serie inteira): depois
//          de respondida, nunca mais pergunta daquela serie.
//   Nunca com o ajuste desligado (Reproducao > "Perguntar o que achou nos
//   creditos"), nunca de titulo ja respondido, uma vez por sessao do player.
//
// ONDE: cartao no canto inferior direito, sem cobrir o video inteiro. Com o
// pos-reproducao no ar ele sobe para cima do painel (posplay_topo) — nunca por
// cima do cartao de proximo episodio nem da fileira de relacionados.
//
// TECLADO: ESQUERDA/DIREITA andam entre as tres opcoes, OK escolhe, VOLTAR
// pula. O cartao so toma essas quatro teclas, e so com os controles do player
// escondidos — com a barra em pe, ESQUERDA/DIREITA continuam sendo o avanco.
// BAIXO nunca e dele (o pos-reproducao usa para devolver a barra).
// Some sozinho em 8 s (a contagem recomeca a cada tecla nele).
//
// NAO RESPONDIDA, A PERGUNTA FICA PENDENTE e aparece de forma discreta na
// pagina do titulo (detail.c): uma linha pequena no canto inferior direito,
// "↑ O que achou de X?". CIMA na linha de botoes abre o mesmo cartao, sem
// contagem. Pendente expira em 30 dias.
//
// A RESPOSTA: grava em reacoes[-p<N>].txt (por perfil), manda ev=reacao
// (atividade.c, so com envio permitido) e, com o Trakt ligado, nota em
// /sync/ratings: Gostei=8, Mais ou menos=5, Nao gostei=2 (decisao do dono).
#ifndef NV_REACAO_H
#define NV_REACAO_H
#include "catalogo.h"
#include <SDL2/SDL.h>

#define REACAO_GOSTEI    1
#define REACAO_MAIS_MENOS 0
#define REACAO_NAO      -1
#define REACAO_PENDENTE  9      // perguntada e sem resposta
#define REACAO_NENHUMA  -9      // nunca perguntada
#define REACAO_TIMEOUT_MS 8000u
#define REACAO_PENDENTE_DIAS 30

// --- pura (teste) -------------------------------------------------------------
// `temProximo`: ha episodio seguinte na lista; `proxOutraTemporada`: ele e de
// outra temporada (este e o fim da temporada). `cred` = marcador de creditos
// (0 = nenhum). Filme ignora os dois primeiros.
int reacao_regra_perguntar(int ehSerie, int temProximo, int proxOutraTemporada,
                           double posSeg, double durSeg, double cred);
// Nota do Trakt para cada resposta (8/5/2); 0 para valor invalido.
int reacao_nota_trakt(int reacao);
// 1 com o cartao no ar no player (para o player escurecer o video embaixo).
int reacao_visivel(void);

// --- estado local ---------------------------------------------------------------
// REACAO_GOSTEI/MAIS_MENOS/NAO, REACAO_PENDENTE ou REACAO_NENHUMA.
int  reacao_estado(const char *imdb);
// Responde (grava, ev=reacao, nota no Trakt). `ci` pode ser NULL se `imdb` vem
// de uma linha ja guardada.
void reacao_responder(const char *imdb, int reacao);

// --- player -----------------------------------------------------------------------
// Por quadro, com o titulo de verdade (nao canal). Abre o cartao no momento.
void reacao_player_atualizar(float dt, Uint32 agora, const CatItem *ci,
                             int ehSerie, double posSeg, double durSeg,
                             double cred, int temProximo, int proxOutraTemporada);
// 1 = consumiu. `controlesVisiveis`: com a barra em pe o cartao nao toma tecla.
int  reacao_evento(const SDL_Event *e, int controlesVisiveis);
// `baseY` e a linha acima da qual o cartao cabe (ja descontado o posplay).
void reacao_desenhar(Uint32 agora, float baseY);
// Fecha sem responder (a pendencia, se houve pergunta, fica). Titulo novo.
void reacao_fechar(void);
int  reacao_aberta(void);

// --- pagina do titulo ----------------------------------------------------------
// 1 quando ha pergunta pendente para `ci` (e o ajuste esta ligado).
int  reacao_detalhe_pendente(const CatItem *ci);
// Abre o cartao (sem contagem) para responder da pagina. 1 = abriu.
int  reacao_detalhe_abrir(const CatItem *ci);
// A linha discreta, no canto inferior direito. `a` = alpha da pagina.
void reacao_detalhe_dica(const CatItem *ci, float a);

// --- aba Amigos: "Ja assisti" numa recomendacao recebida -----------------------
// O MESMO cartao, modal e sem contagem, centrado em `cx` (o meio do painel; <0
// = meio da tela). Passo 1: gostei / mais ou menos / nao gostei (vai a quem
// mandou e vale como a reacao do titulo); passo 2: "Valeu pela dica!",
// "Escrever mensagem" (teclado de tela) ou "Agora nao". Voltar fecha sem
// responder — a rec continua assistida. 1 = abriu.
int  reacao_rec_abrir(long long rec, const char *imdb, const char *titulo, const char *midia,
                      const char *poster, const char *nome, float cx);
int  reacao_painel_aberta(void);
// Quem e dono da tela (o painel) chama os dois por quadro e entrega as teclas a
// reacao_evento(e, 0) enquanto reacao_painel_aberta().
void reacao_painel_atualizar(float dt, Uint32 agora);
void reacao_painel_desenhar(Uint32 agora);
// 0 = pergunta da reacao, 1 = "mandar uma mensagem?", -1 = fechado. Teste.
int  reacao_passo(void);
// Teste do ponteiro (#99): a resposta em foco (0..2).
int  reacao_teste_foco(void);

// Para o teste de tela: abre o cartao como no player, com origem opcional.
void reacao_teste_abrir(const char *imdb, const char *titulo, const char *midia,
                        long long rec, const char *nomeRec, int envia);
#endif
