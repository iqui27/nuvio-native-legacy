// GIF ANIMADO nas capas de colecao.
//
// As colecoes que vem do PACOTE animam por uma sequencia de JPEG numerado
// (folder->frames + frameDir/001.jpg), tocada a ~15 fps em home.c. As que vem
// da CONTA entregam um GIF, e ai a animacao nao acontecia: o app manda todo
// arquivo para um decodificador que devolve UM quadro, entao a capa virava a
// primeira imagem parada. E o issue #29.
//
// ONDE ANIMA E ONDE NAO ANIMA, e o porque:
//
//   Tizen  — anima. O navegador ja sabe animar GIF sozinho: basta uma <img>
//            com o arquivo e pedir a ela o quadro que estiver valendo. Quem
//            conta o tempo e faz a composicao dos quadros e o proprio Chromium.
//
//   webOS  — NAO anima, e devolve 0. O SDL2_image da TV so exporta
//            IMG_LoadGIF_RW (um quadro; nao ha IMG_LoadAnimation, conferido no
//            aparelho com `nm -D`), e nao existe libgif no sistema — so
//            libwebp, que e como webp.c se safa. Animar la exige um
//            decodificador LZW proprio, que e trabalho de verdade e nao esta
//            escrito. O chamador desenha a capa parada, como hoje.
#ifndef NV_GIF_H
#define NV_GIF_H
#include "gl_compat.h"

// O arquivo e um GIF com MAIS DE UM quadro? Le so a estrutura de blocos, que e
// toda prefixada por tamanho — nao decodifica pixel nenhum.
int gif_animado(const char *caminho);

// Textura com o quadro que a animacao esta mostrando AGORA, ou 0 quando o alvo
// nao anima, o arquivo nao e GIF animado, ou o navegador ainda nao carregou.
// A textura e reaproveitada entre chamadas: e sempre a mesma, com o conteudo
// trocado. So um cartaz anima por vez (o que esta em foco), e e nisso que esta
// funcao se apoia para nao guardar quadro nenhum.
GLuint gif_textura(const char *caminho, int largAlvo);

// Solta o que estiver preso ao caminho corrente. Chamar quando o foco sai.
void gif_parar(void);

#endif
