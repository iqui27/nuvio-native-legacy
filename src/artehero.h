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
// QUALIDADE DA IMAGEM: 0 baixa, 1 padrão, 2 alta (vem da tela de Ajustes).
// Na baixa, a arte de tela cheia é a url que o catálogo guarda — sem subir para
// a versão grande. É a diferença entre baixar 3840 px e baixar 1280.
void artehero_qualidade(int nivel);

const char *artehero_url(const CatItem *item);

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

#endif
