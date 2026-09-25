// "TROCAR ARTE": a tela onde a pessoa escolhe o fundo e o logo de um titulo
// (#142). Aberta pelo circular de moldura na linha de acoes da pagina do
// titulo (detail.c, ACAO_ARTE); a escolha fica em arteescolha.h e vale para
// o destaque da home, a tela cheia, o detalhe e o player (artehero.h).
//
// COMO SE USA: duas abas, "Fundos" (miniaturas 16:9) e "Logos". A primeira
// miniatura de cada aba e "Automatico" — a regra de sempre, e escolhe-la
// apaga a escolha. Andar pelas miniaturas TROCA O FUNDO DA PAGINA ao vivo
// (o texto da pagina sai, fica a arte e o logo): o que se ve e o resultado.
// OK escolhe e fecha; Voltar fecha sem mudar nada.
//
// DE ONDE VEM CADA MINIATURA, na ordem:
//   Fundos  Automatico, catalogo (o `background` do addon), metahub pelo id do
//           IMDb, arte-chave da Apple TV, fanart.tv (so com a chave pessoal) e
//           a lista do TMDB /images — sem idioma primeiro, depois por nota.
//   Logos   Automatico, catalogo, metahub e os logos do TMDB no idioma da
//           interface, sem idioma e em ingles (include_image_language).
// Apple e fanart entram como url VIRTUAL (artereserva.h): a consulta so sai
// quando a miniatura e pedida, no fio de rede do tex_cache. Fonte que nao
// tem a arte (404, sem chave) some da grade quando a textura falha. Sem chave
// do TMDB a lista dele nao vem, e o resto aparece do mesmo jeito.
//
// CUSTO: nada acontece antes de abrir. Aberta, um fio pergunta ao TMDB (uma
// ou duas viagens) e as miniaturas pedem w300 (artetamanho.h reescreve o
// tamanho no fio de rede); a arte de tela cheia da previa so e pedida depois
// de o foco parar ~200 ms numa miniatura, para a seta segurada nao disparar
// um backdrop de 1920 por passo.
#ifndef NV_TROCAARTE_H
#define NV_TROCAARTE_H
#include <SDL2/SDL.h>
#include "catalogo.h"

void trocaarte_abrir(const CatItem *item);
void trocaarte_fechar(void);
int  trocaarte_aberto(void);
// 0..1, a mola de entrada/saida: a pagina apaga o texto por ela.
float trocaarte_visivel(void);
void trocaarte_evento(const SDL_Event *e);
void trocaarte_atualizar(float dt);
// `logoPagina` e o logo que a pagina esta usando (para a aba de fundos, onde
// o logo nao muda). Desenha por cima da arte, abaixo de mais nada.
void trocaarte_desenhar(const char *logoPagina);

// A url de FUNDO que a pagina deve desenhar agora (a previa ja carregada), ou
// NULL para seguir a regra normal. So vale com a tela aberta na aba Fundos.
const char *trocaarte_previa_fundo(void);
// 1 uma vez depois de um OK que mudou a escolha: a pagina solta a arte que
// tinha congelado na abertura (arteFixa/logoFixo) e pede de novo.
int  trocaarte_consumir_mudanca(void);

// --- teste ---------------------------------------------------------------
// Poe uma miniatura na aba (0 fundos, 1 logos) sem rede. `rotulo` e copiado.
void trocaarte_teste_candidato(int aba, const char *url, const char *rotulo);
// Aba e foco (indice na lista visivel) direto, como se o D-pad tivesse andado.
void trocaarte_teste_foco(int aba, int pos);
int  trocaarte_n(int aba);

#endif
