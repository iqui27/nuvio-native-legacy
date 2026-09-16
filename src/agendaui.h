// Tela AGENDA: a LINHA DO TEMPO das series que o dono acompanha.
//
// Uma coluna, um eixo vertical, uma estacao por serie, ordenada pela data do
// proximo episodio. O D-pad so sobe e desce; OK liga ou desliga o lembrete
// daquela serie. Nao ha segundo nivel de navegacao de proposito: a tela responde
// a uma pergunta so ("o que sai, e quando"), e abrir o titulo daqui e o que a
// Biblioteca ja faz.
//
// Os dados vem inteiros de agenda.c — esta tela nao sabe de rede, de TMDB nem
// de arquivo. Ela pede agenda_montar() ao abrir e desenha o que voltar.
#ifndef NV_AGENDAUI_H
#define NV_AGENDAUI_H
#include <SDL2/SDL.h>
#include "gfx.h"
#include "text.h"

int  agendaui_iniciar(void);
void agendaui_evento(const SDL_Event *e);
void agendaui_atualizar(float dt, Uint32 agora);
void agendaui_desenhar(Uint32 agora);
int  agendaui_quer_sair(void);   // 1 quando o Back deve fechar a tela

// O DESPERTADOR, desenhado em qualquer tela que fale de lembrete (a estacao da
// agenda, o botao do hero, o cartao de abertura). Mora aqui porque as tres
// precisam do MESMO desenho: tres copias divergem, e a divergencia aparece
// como "o sino da tela X e outro".
//
// Pedido do dono: "o do lembrete tb tem que ser um relogio daqueles de alarme
// e tem que ser animado, para nao ser igual aos outros botoes". O icone e um
// PNG de deploy/app/art/icones (art/icones/lembrete.svg versionado ao lado,
// mesmo traco dos vizinhos — nao ha renderizador de SVG neste app); o que
// anima sao a POSICAO dele e as ondas de som ao lado, desenhadas com as
// primitivas do gfx.
//
// O DESENHO FOI REFEITO depois de "ta muito pesado, quero minimalista e
// elegante": as campainhas de circulo cheio viraram os dois tracos inclinados
// do topo e o glifo voltou para a escala da fileira. O porque, com as medidas
// que levaram a isso, esta em lembrete.svg e em detail.c — aqui so importa que
// e o MESMO arquivo nas tres telas, entao ninguem precisa conferir tres vezes.
//
// `ligado` = o lembrete esta marcado. A COR vem por cr/cg/cb, e nao mais por
// uma luminancia unica: desde que o estado ligado e VERDE (pedido do dono,
// "quando tiver ativo ele ficar um verdinho bonito"), um so canal nao consegue
// exprimi-lo. Use agendaui_cor_lembrete para nao espalhar o verde pelo codigo.
// `desde` e o SDL_GetTicks em que o dono ligou — 0 quando nao se sabe; serve ao
// tremor forte do instante da troca, que e o unico momento em que ele esta
// olhando.
//
// Com "reduzir animacoes" ligado nao treme nem pulsa, e o estado continua
// legivel: as ondas ficam paradas e opacas. Ver ajustes_animacoes_reduzidas.
void agendaui_despertador(GfxRect r, int ligado, float cr, float cg, float cb,
                          float a, Uint32 agora, Uint32 desde);

// A COR DO DESPERTADOR, num lugar so.
//
// O verde e o EMERALD #66bb6a que ja esta na paleta de acentos (ajustes.c:204),
// nao um verde novo — o mesmo argumento que o selo do Social ja registra em
// salvospainel.c. Sobre SUPERFICIE CLARA (o botao focado, a estacao focada) ele
// nao serve: #66bb6a contra #f5f5f5 da 2,2:1, abaixo dos 3:1 que a AA pede ate
// para simbolo grande. Ali entra o MESMO verde escurecido, que da 8:1 e continua
// sendo verde — escurecer nao muda o matiz, e era o matiz que o dono pediu.
//
// A COR NUNCA E O UNICO SINAL: o despertador ligado tambem ganha as ondas de
// som e o tremor, e o desligado nao tem nem uma nem outro. Quem nao distingue
// verde de cinza continua lendo o estado pela FORMA. Numa TV a tres metros isso
// nao e teoria: e a diferenca entre ver o estado e adivinhar.
void agendaui_cor_lembrete(int ligado, int sobreClaro,
                           float *r, float *g, float *b);

// A SINOPSE DO EPISODIO EM ATE `maxLinhas`, terminando em reticencias quando
// nao coube. Mora aqui porque as duas telas que mostram sinopse de episodio sao
// esta e o cartao do lembrete, e elas tem de cortar igual. Devolve quantas
// linhas foram desenhadas (0 quando nao ha texto).
//
// Por que nao txt_bloco: ele corta NO SECO no numero de linhas, sem reticencia
// (text.c e mantido por outro agente e nao e este trabalho que muda a
// primitiva). Em cinco linhas — o uso da tela de titulo — ninguem nota. Em DUAS
// a frase acaba no meio ("...precisa decidir se o") e le como defeito de
// desenho, nao como resumo. Aqui as linhas cheias sao preenchidas palavra a
// palavra com a mesma medida de txt_linha, e o que sobra vai para
// txt_linha_corta, que ja sabe fechar com "…".
//
// `maxLinhas` E PARAMETRO desde que a Agenda passou a desenhar a sinopse numa
// coluna estreita a direita da linha: la cabem tres linhas de 30px dentro da
// altura da propria linha, enquanto o cartao do lembrete continua com duas.
// Fixar duas aqui obrigaria a Agenda a cortar a frase na virgula errada.
int agendaui_sinopse(TxtEstilo estilo, const char *s, float x, float y,
                     float larg, float leading, int maxLinhas,
                     int r, int g, int b, float alpha);

// QUANTAS LINHAS a chamada acima vai desenhar, sem desenhar nada. Existe porque
// o bloco da direita da Agenda e CENTRADO na altura da linha, e centrar exige
// saber a altura antes. Passe a MESMA cor que vai desenhar: a largura nao
// depende dela, mas o cache de txt_linha e indexado por cor e medir noutra cor
// rasteriza uma segunda copia de cada linha.
int agendaui_sinopse_linhas(TxtEstilo estilo, const char *s, float larg,
                            int maxLinhas, int r, int g, int b);

#endif
