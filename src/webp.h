// WebP por dlopen da libwebp DO APARELHO. O SDL2_image da TV nao foi compilado
// com WebP ("WEBP images are not supported"), mas /usr/lib/libwebp.so.7 existe
// no sistema. As colecoes da conta vem do CDN do Xperience em .webp — 149 de
// 169 pastas — e sem isto a home inteira delas era cartao vazio.
#ifndef NV_WEBP_H
#define NV_WEBP_H
#include <SDL2/SDL.h>
// Superficie ABGR8888 nova (o chamador libera) ou NULL quando nao e WebP, a
// lib nao existe ou a decodificacao falhou. Nunca imprime em caso "nao e WebP".
SDL_Surface *webp_carregar(const char *caminho);

#ifdef __EMSCRIPTEN__
// A PONTE PARA O DECODIFICADOR DO NAVEGADOR, para qualquer formato que ele
// leia (`mime`: image/jpeg, image/png, image/webp). Bloqueia o fio chamador
// (nunca o principal). Devolve RGBA de malloc com largura <= largMax quando
// largMax > 0 — a reducao acontece no canvas, com o bitmap inteiro FORA do
// heap do WASM; `ow`/`oh` recebem o tamanho original. Ver jpegrapido.c para
// o porque: no Tizen o IMG_Load em software de um 3840x2160 levava 10-17 s e
// 33 MB do heap de 256 MiB (logs do #69/#68).
#include <stdint.h>
#include <stddef.h>
uint8_t *navegador_decodificar(const unsigned char *dados, size_t n, const char *mime,
                               int largMax, int *lw, int *lh, int *ow, int *oh);
#endif
#endif
