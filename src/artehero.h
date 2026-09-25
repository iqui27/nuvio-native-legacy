// A ARTE QUE OCUPA A TELA: qual URL pedir quando o desenho é de 1920 px.
//
// O catálogo guarda UMA url de fundo por título, e ela vem do que a fonte
// mandou: o Cinemeta entrega o backdrop do metahub (1920x1080), o TMDB entrega
// um caminho que a descoberta reescreve para w1280 (1280x720) porque é o
// tamanho certo para um CARD. Desenhar w1280 a 1920 amplia 1,5x, e é isso que
// se vê como "arte pixelada no destaque" — o arquivo está inteiro, ele só é
// pequeno para o lugar onde está sendo usado.
//
// Aqui mora a política de qual url pedir QUANDO O DESENHO É DE TELA CHEIA. Não
// é um cache e não baixa nada: recebe o item e devolve a melhor url conhecida,
// num buffer estático próprio.
//
// POR QUE NÃO ARRUMAR NA DESCOBERTA: a mesma url alimenta o cartão da fileira,
// onde 1280 é grande demais e `original` (3840) seria desperdício de decode
// medido em centenas de milissegundos. Quem sabe o tamanho do desenho é quem
// desenha.
#ifndef NV_ARTEHERO_H
#define NV_ARTEHERO_H

#include "catalogo.h"
#include <stddef.h>

// Melhor url de FUNDO para desenho de tela cheia. Devolve NULL quando o item
// não tem arte nenhuma. O ponteiro é estático: use antes da próxima chamada.
// COMO SABER QUE UMA ARTE NÃO VEM.
//
// A política prefere a url MAIS BARATA que chega em 1920 — e a mais barata nem
// sempre existe para aquele título. Sem uma resposta a "essa falhou?", a única
// saída seria pedir sempre a mais cara, que é o que fez o destaque baixar
// 3840x2160 do TMDB quando o metahub já servia 1920 a um quarto do decode.
//
// Recebe a função em vez de incluir tex_cache.h de propósito: assim este módulo
// continua sendo política de url, testável sem SDL e sem GL. Sem ninguém
// registrar nada, nada falhou — que é o comportamento certo para um teste.
void artehero_definir_falhou(int (*falhou)(const char *caminho));

// QUALIDADE DA IMAGEM: 0 baixa, 1 padrão, 2 alta (vem da tela de Ajustes).
// Na baixa, a arte de tela cheia é a url que o catálogo guarda — sem subir para
// a versão grande. É a diferença entre baixar 3840 px e baixar 1280.
void artehero_qualidade(int nivel);

const char *artehero_url(const CatItem *item);

// FONTE DO FUNDO (ajuste "Background do hero"). Os numeros sao o INDICE
// gravado em ajustes.txt ("heroFundoLocal N") — ordem e contrato.
#define ARTEHERO_AUTO     0
#define ARTEHERO_CATALOGO 1   // o `background` que o addon/Cinemeta mandou
#define ARTEHERO_METAHUB  2   // images.metahub.space pelo id do IMDb
#define ARTEHERO_TMDB     3   // backdrop do TMDB (virtual por id quando falta)
#define ARTEHERO_TRAKT    4   // fanart do Trakt (idem)
// 23/09/2026, NO FIM (o indice e o gravado):
#define ARTEHERO_APPLE    5   // arte-chave da Apple TV (busca do trailer)
#define ARTEHERO_FANART   6   // fanart.tv, so com a chave pessoal (Ajustes)
#define ARTEHERO_ANIME    7   // Kitsu/AniList, so para anime
#define ARTEHERO_N_ESCOLHAS 8 // quantas aparecem em "Background do hero"
// INTERNO, fora dos Ajustes: o OUTRO backdrop do TMDB (/images, sem texto,
// que nao e o padrao). E o que "TMDB" vira com "Destaque com outra arte"
// ligado — medido em 23/09, o padrao do TMDB e o fanart do Trakt costumam ser
// a MESMA foto do fundo do metahub (o card do Cinemeta), so reencodada.
#define ARTEHERO_TMDB_OUTRO 8

// fanart.tv so existe com chave: sem ela a fonte devolve NULL e o destaque
// cai na seguinte. Chamado por Ajustes ao ler/trocar a chave.
void artehero_fanart_disponivel(int sim);

// "Estas duas urls sao a MESMA imagem?" alem da comparacao por caminho —
// o main registra arte_mesma_imagem (bytes baixados, artereserva.h). Sem
// registro, so o caminho conta (o teste roda assim).
void artehero_definir_igual(int (*igual)(const char *a, const char *b));
// A url real de uma virtual ja resolvida, sem rede (arte_fonte_resolvida).
void artehero_definir_resolvida(int (*f)(const char *url, char *saida, size_t tam));

// Fundo de TELA CHEIA da fonte pedida, ou NULL quando o item nao tem como ter
// essa fonte (sem id do IMDb e sem a url dela). TMDB/Trakt sem url no item
// devolvem uma url virtual (artereserva.h). `fonte` 0 = artehero_url().
const char *artehero_url_fonte(const CatItem *item, int fonte);

// A REGRA DO CARD E DO DESTAQUE (ajustes "Background do hero" e "Destaque
// com outra arte", 22/09):
//
//   Outra arte DESLIGADO (padrao, o pedido de 19/09): card deitado, destaque
//   e detalhe mostram A MESMA FOTO — a da fonte escolhida, ou a do catalogo
//   no Automatico. O destaque pede o tamanho grande da mesma foto, o card o
//   dele; no padrao de qualidade e o mesmo arquivo (zero download novo).
//
//   Outra arte LIGADO: o card fica com a arte do CATALOGO; destaque e detalhe
//   usam a fonte escolhida (TMDB vira o OUTRO backdrop do TMDB). Se ela for
//   Automatico, nao existir para o titulo, ja tiver falhado ou for a mesma
//   foto do card (mesmo caminho ou mesmos bytes), vale a primeira OUTRA foto
//   na ordem Apple TV, TMDB outro, fanart.tv, anime, Trakt, TMDB,
//   IMDb/Metahub, catalogo. Sem nenhuma, a do card.
//
// Cartaz em pe nao muda: a escolha e de FUNDO, e o retrato nao tem par nas
// fontes de fundo. Uma url que ja falhou (tex_falhou) nunca e devolvida
// quando existe outra — o desenho nao fica preso num 404.
const char *artehero_url_card_fonte(const CatItem *item, int fonte, int diferente);
const char *artehero_url_destaque(const CatItem *item, int fonte, int diferente);

// A url que o item guarda, sem política — para quem desenha pequeno.
const char *artehero_url_card(const CatItem *item);

// O STILL DO EPISÓDIO EM ANDAMENTO, em tamanho de tela cheia, ou NULL.
//
// Pedido do dono: "quando for o episódio, coloca a foto do episódio". Um item
// de "Continuar assistindo" de série é um EPISÓDIO — e mostrar a arte genérica
// da série no destaque desperdiça a única imagem que diz onde a pessoa parou.
//
// A url é determinística e não custa consulta: o metahub responde por
// id+temporada+episódio e redireciona para o TMDB. MEDIDO em 17/09 com curl na
// mesma série: `.../w780.jpg` chega a 780x439, `.../w1280.jpg` a 1280x720 e
// `.../original.jpg` a 1920x1080 — por isso a de tela cheia pede `original`.
//
// NEM TODO EPISÓDIO TEM STILL: o log da TV já mostrava 404 para alguns. Quem
// desenha confere com tex_falhou() e cai na arte do título; aqui só se monta a
// url de quem tem id do IMDb e episódio conhecido.
const char *artehero_url_episodio(const CatItem *item);

// O LOGO DO TITULO no tamanho do desenho. Devolve a url recebida quando ela
// nao e do TMDB (metahub e arquivo do pacote passam intactos). O `original`
// que o Cinemeta manda tem 4127 px de largura para um desenho de 1000 — a
// medicao esta no .c.
const char *artehero_url_logo(const char *logo);
// O mesmo, sabendo a largura em que o logo vai ser desenhado (px de layout):
// escolhe o menor tamanho do TMDB que cobre o desenho. 0 = como acima.
const char *artehero_url_logo_larg(const char *logo, float larg);

// Seleção visual da sessão: detalhe/player e Home compartilham a primeira URL
// canônica do mesmo IMDb/tipo. Outra identidade nunca herda esse snapshot.
// Sem logo na abertura, a primeira logo que chegar pode preenchê-lo.
void artehero_logo_sessao_iniciar(const CatItem *item);
const char *artehero_logo_sessao(const CatItem *item);
// Registra o logo do hero que a Home está efetivamente desenhando. Cards
// vizinhos não chamam esta função e, portanto, não substituem a seleção.
const char *artehero_logo_sessao_observar(const CatItem *item);
// Usa o snapshot quando ele pertence ao item; fora dele mantém a política de
// largura do desenho do card.
const char *artehero_logo_sessao_larg(const CatItem *item, float larg);

// A ARTE ESCOLHIDA A MAO (#142, arteescolha.h) VENCE TODA A POLITICA ACIMA.
//
// Registrada pelo main como as outras (definir_falhou, definir_igual): assim
// este modulo continua sem depender de disco, e o teste dele segue compilando
// sozinho. As duas funcoes recebem o id do item (imdb, com ou sem episodio) e
// devolvem a url escolhida ou NULL (Automatico).
//
// ONDE VALE: artehero_url e artehero_url_destaque (destaque da home, tela
// cheia, detalhe, player, Continuar assistindo — o still do episodio perde
// para ela, ver arte_hero_do_item em home.c) e os logos de sessao (hero, card
// aberto, detalhe, player). O CARD DEITADO NAO MUDA: a escolha e de tela
// cheia, e o card continua com o arquivo que a fileira ja baixou.
//
// Escolha que ja falhou (tex_falhou) nao prende a tela num 404: vale NULL, e
// a politica automatica volta, sem apagar a escolha do disco.
void artehero_definir_escolha(const char *(*fundo)(const char *id),
                              const char *(*logo)(const char *id));
// O fundo escolhido para este item no tamanho de tela cheia, ou NULL.
const char *artehero_url_escolhida(const CatItem *item);
// Uma url de fundo guardada, no tamanho de tela cheia (TMDB pela qualidade).
const char *artehero_url_escolha_grande(const char *u);
// O logo escolhido, ja no tamanho de artehero_url_logo, ou NULL.
const char *artehero_logo_escolhido(const CatItem *item);
// A tela de escolha precisa ver o AUTOMATICO de um titulo que ja tem escolha
// (a miniatura "Automatico"): entre suspender(1) e suspender(0) as funcoes
// acima devolvem NULL.
void artehero_escolha_suspender(int sim);
// A chave com que a escolha deste item e guardada: o imdb (com episodio, que
// arteescolha corta) ou, sem ele, "tmdb:m<id>"/"tmdb:t<id>". "" = sem chave.
void artehero_id_escolha(const CatItem *item, char *dst, size_t n);

#endif
