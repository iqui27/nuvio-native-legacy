// PÓS-REPRODUÇÃO: o que aparece quando o título está acabando.
//
// Portado do `postPlayRecommendationController` do app web 1.0.6, que por sua
// vez veio do Android TV. As regras de QUANDO aparecer sao as de la, medidas no
// fonte e nao escolhidas aqui:
//   - FILME: quando o progresso passa de 90% da duracao
//     (DEFAULT_POST_PLAY_MOVIE_THRESHOLD_PERCENT).
//   - SERIE: nos ultimos segundos, com contagem regressiva de 5
//     (POST_PLAY_RECOMMENDATION_FINAL_COUNTDOWN_SECONDS).
//   - Em qualquer caso, no fim do fluxo.
//
// O QUE MOSTRA e diferente do web, e de proposito:
//   - SERIE -> o PROXIMO EPISODIO, com contagem para tocar sozinho. E o que
//     mais vale numa serie, e temos o dado: a lista de episodios agora e unica
//     e cobre todas as temporadas.
//   - FILME -> os RELACIONADOS que o extras.c ja busca do Trakt ao abrir o
//     titulo. Sem trailer: nao ha reprodutor de YouTube neste port, e o web usa
//     um proxy local que nos nao temos.
#ifndef NV_POSPLAY_H
#define NV_POSPLAY_H
#include <SDL2/SDL.h>

// Chamada por quadro pelo player, com a posicao e a duracao correntes.
// `janelaSerie` diz se a serie ja esta na janela de "proximo episodio" — os
// creditos rolando ou os dois minutos finais. Quem sabe isso e o player, que le
// os marcos de introducao; o painel so decide o que mostrar.
void posplay_atualizar(float dt, Uint32 agora, double posSeg, double durSeg,
                       int ehSerie, int idxCatalogo, int janelaSerie);
int  posplay_visivel(void);
// A regra do FILME sozinha, sem estado: 1 quando os relacionados devem subir.
// `creditosSeg` e o marcador (0 = nenhum). Nunca antes da metade da duracao;
// marcador fora do ultimo quarto e recusado (#115). posplay_atualizar soma a
// ela a exigencia de a duracao estar estavel.
int  posplay_regra_filme(double posSeg, double durSeg, double creditosSeg);

// Abre os relacionados a pedido (o botao do player), sem esperar o fim do
// filme. Devolve 0 quando nao ha relacionado nenhum para mostrar. Necessario
// porque a dispensa gruda: sem esta porta, um Voltar apagaria os relacionados
// daquele filme para sempre.
int  posplay_abrir_relacionados(int idxCatalogo);
// 0 nao consumiu; 1 consumiu; 2 consumiu E o player deve mostrar os controles
// (e o BAIXO: tira o painel do caminho e devolve a barra de tempo).
int  posplay_evento(const SDL_Event *e);
// `baseY` e a linha ACIMA da qual o painel cabe inteiro — o topo do que o
// player ja desenha. Ancorar pela base, e nao por um y fixo, e o que impede o
// painel de cair em cima da barra de tempo.
void posplay_desenhar(Uint32 agora, float baseY);
// Fecha e zera. Chamado quando o player abre outra coisa.
void posplay_fechar(void);

// Pedidos para o roteador, consumidos uma vez:
// proximo episodio (temporada/episodio) ou titulo relacionado (indice).
int  posplay_pediu_episodio(int *temporada, int *episodio);
int  posplay_pediu_titulo(void);
#endif
