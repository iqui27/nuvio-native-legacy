#ifndef NV_SDLCOMPAT_H
#define NV_SDLCOMPAT_H

#include <SDL2/SDL.h>

/*
 * Superficie de 32 bits num formato de pixel nomeado, sem
 * SDL_CreateRGBSurfaceWithFormat.
 *
 * POR QUE ISTO EXISTE. Comparando os simbolos indefinidos do binario ARM contra
 * os dumps de firmware retail da webosbrew (dev-toolbox-cli, common/data),
 * tomando a C9 como linha de base porque e onde o app comprovadamente roda, das
 * 199 entradas exatamente UMA nao e exportada pelas TVs antigas:
 *
 *   webOS 3.4.0 (2016, W16N)  falta SDL_CreateRGBSurfaceWithFormat
 *   webOS 3.9.2 (2017, W17H)  falta SDL_CreateRGBSurfaceWithFormat
 *   webOS 2.2.3 (2015, W15M)  falta SDL_CreateRGBSurfaceWithFormat
 *   webOS 4.10.0 (2019, W19P) nada falta
 *
 * Ela entrou na SDL 2.0.5. A webOS 3.4 traz a 2.0.2, a 3.9 traz a 2.0.4, e a
 * C9 traz uma 2.0.4 que a LG evidentemente remendou, porque exporta o simbolo.
 * Um unico simbolo era toda a distancia entre este binario e uma TV de 2016.
 *
 * O caminho abaixo existe desde a SDL 2.0.0 e esta presente nos tres dumps
 * antigos. Substitui os tres pontos de chamada INCONDICIONALMENTE, e nao atras
 * de um dlsym, de proposito: assim a C9 exercita exatamente o mesmo codigo que
 * a TV antiga vai rodar, em vez de deixar um ramo que so existe onde ninguem
 * consegue testar.
 *
 * SDL_PixelFormatEnumToMasks em vez das mascaras escritas a mao: ABGR8888 tem
 * ordem de byte diferente conforme o endianness, e a SDL ja sabe a resposta.
 */
static SDL_INLINE SDL_Surface *nv_superficie(Uint32 bandeiras, int w, int h, int bits,
                                             Uint32 formato) {
  int profundidade = 0;
  Uint32 rm = 0, gm = 0, bm = 0, am = 0;
  if (!SDL_PixelFormatEnumToMasks(formato, &profundidade, &rm, &gm, &bm, &am)) {
    return NULL;
  }
  (void)bits; /* mantido na assinatura para casar com os pontos de chamada */
  return SDL_CreateRGBSurface(bandeiras, w, h, profundidade, rm, gm, bm, am);
}

#endif
