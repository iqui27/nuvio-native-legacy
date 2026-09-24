// A ARTE NO TAMANHO DO DESENHO (23/09/2026).
//
// POR QUE EXISTE: log de campo (LG 1.7 GB, 1.4.3) com
//   [tex] decode lento: 374 ms (...) para 3840x2160 (saiu 480x270)
//         https://image.tmdb.org/t/p/original/fIhXD6m9LJgC7yDMOHMpYgkYAJ.jpg
// Capa de COLECAO da conta (colecoes.c: coverImageUrl) apontando para o
// `original` do TMDB: 695 KB e 8,3 MP para um card de 480. O TMDB serve o
// MESMO arquivo em w780 com 71 KB (medido com curl em 23/09: w300 300x169,
// w780 780x439, w1280 1280x720, original 3840x2160 — e em cartaz w300, w500,
// w780 e w1280 tambem respondem). O decode escalado (jpegrapido.c) reduz na
// DCT, mas ainda le o arquivo inteiro: e o "ler 374".
//
// O QUE FAZ: dada a URL e o teto de decode do pedido (Item.limite em
// tex_cache.c — a largura em pixels que a textura vai ter), devolve a menor
// variante do MESMO arquivo que ainda cobre o teto. So hosts cuja escada de
// tamanhos esta medida:
//   image.tmdb.org/t/p/<w300|w780|w1280|original|wNNN>/...
//   episodes.metahub.space/<tt>/<t>/<e>/<w780|w1280|original>.jpg
//   images.metahub.space/background/medium/... -> small (480x270) com teto
//     <= 480; medium, big, large e original sao o mesmo 1920x1080 (artehero.c).
// Host desconhecido (as capas PNG de 3840 no r2.dev, por exemplo) fica intacto:
// nao ha tamanho menor a pedir.
//
// Nunca desce ABAIXO do teto (o card nao amplia) e so troca quando a variante
// e ao menos 1,5x menor que a atual: w342 -> w300 e cache novo sem ganho.
//
// QUEM CHAMA: tex_cache.c, no fio de rede, com o teto do pedido. A CHAVE do
// item continua sendo a URL original; so o download (e o arquivo no cache de
// disco) e da variante. A promocao a heroi pede de novo com teto maior e cai
// na variante maior — ou na URL original.
#ifndef NV_ARTETAMANHO_H
#define NV_ARTETAMANHO_H
#include <stddef.h>

// 1 e `saida` preenchida quando ha variante menor que cobre `limite`; 0 quando
// a URL deve ser baixada como veio (host desconhecido, tamanho ja justo,
// teto maior que a maior variante, `limite` <= 0, `saida` pequena).
int arte_tamanho_url(const char *url, int limite, char *saida, size_t tam);

#endif
