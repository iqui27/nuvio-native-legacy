// DECODE DE JPEG NO TAMANHO QUE VAI SER USADO, e nao no tamanho do arquivo.
//
// POR QUE: MEDIDO na C9 em 19/09/2026, um card de 736 px feito de um fundo
// de 1920x1080 custava 400 a 700 ms no fio de decode — IMG_Load lia os 2
// milhoes de pixels inteiros e tex_reduzir passava por todos eles de novo. A
// libjpeg sabe decodificar a 1/2, 1/4 e 1/8 DENTRO da DCT (scale_num/denom):
// um 1920 pedido a 736 sai a 960x540 direto do decodificador, com um quarto
// dos pixels para ler e para reduzir. Nao e "pular pixels": e o mesmo dado
// da transformada, so sem reconstruir o que vai ser jogado fora.
//
// LIGACAO POR dlopen, e nao -ljpeg: o SDK do webOS traz libjpeg.so.8 (turbo
// 2.1.5, API 8) e a TV so tem libjpeg.so.62 (turbo 1.5.0, ABI 6b) — ligar
// contra o SDK morreria no arranque. O binario abre a .so.62 em tempo de
// execucao com o cabecalho vendorado em src/vendor/jpeg62 (o do turbo com
// JPEG_LIB_VERSION 62, que e o layout da 6b). No Mac abre a libjpeg do brew
// com o cabecalho do brew. Sem biblioteca, sem simbolo ou com "parameter
// struct mismatch", devolve NULL e o chamador cai no IMG_Load de sempre —
// este modulo so encurta o caminho, nunca o fecha.
//
// Nao existe no alvo Tizen (Emscripten): la o decode e do navegador.
#ifndef NV_JPEGRAPIDO_H
#define NV_JPEGRAPIDO_H
#include <SDL2/SDL.h>

// Decodifica `caminho` num SDL_Surface ABGR8888 com largura >= largMax quando
// a origem permite (a maior reducao de DCT que ainda cobre o pedido).
// `larguraOriginal`/`alturaOriginal` recebem o tamanho do ARQUIVO, nao o do
// que saiu — e o que tex_cache guarda em fonteW para decidir promocao. NULL
// quando nao e JPEG, a biblioteca nao esta, ou o decode falhou.
SDL_Surface *jpeg_rapido_carregar(const char *caminho, int largMax,
                                  int *larguraOriginal, int *alturaOriginal);

#ifdef __EMSCRIPTEN__
// So no Tizen: os mesmos formatos (JPEG, PNG, WebP), a partir de bytes ja na
// memoria. NULL para GIF e o resto. Ver a nota em jpegrapido.c.
#include <stddef.h>
SDL_Surface *jpeg_rapido_carregar_mem(const unsigned char *dados, size_t n, int largMax,
                                      int *larguraOriginal, int *alturaOriginal);
#endif
#endif
