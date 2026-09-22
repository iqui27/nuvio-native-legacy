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

// Fonte escolhida para o background do hero:
// 0 automático, 1 catálogo/Cinemeta, 2 IMDb/Metahub, 3 TMDB, 4 Trakt.
// A variante precisa ter chegado com o item; quando não existir, devolve NULL
// para o chamador cair na política automática.
const char *artehero_url_fonte(const CatItem *item, int fonte);

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

#endif
