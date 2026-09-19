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


/*
 * BLINDAGEM DOS FORMATOS DE PIXEL COMPARTILHADOS — o conserto do crash do
 * issue #65 ("o app fecha sozinho depois de um tempo na home / no fim da
 * fileira / quando a TV entra no relogio de economia").
 *
 * O QUE O CORE MOSTROU (C9, 19/09/2026 01:26, build e673b39): SIGABRT com
 * "free(): invalid pointer" dentro de SDL_FreeSurface, chamado por
 * tex_bombear no fio principal, com dois fios de decode dentro de tex_reduzir
 * no mesmo instante. A superficie ja tinha refcount 0 e o ponteiro invalido
 * apontava para dentro do malloc_state de uma arena de fio.
 *
 * A CAUSA NAO E NOSSA E NAO E NOVA: na SDL 2.0.x, SDL_AllocFormat e
 * SDL_FreeFormat mantem uma lista global de SDL_PixelFormat COMPARTILHADOS
 * (um por enum nao indexado) e o refcount deles e ++ e -- SEM trava nem
 * atomico (libsdl-org/SDL issue #2475, "SDL_FreeSurface is NOT thread safe",
 * corrigido so na 2.0.22). Toda superficie criada ou liberada — IMG_Load,
 * SDL_CreateRGBSurface, SDL_ConvertSurfaceFormat, SDL_ConvertPixels (que cria
 * duas superficies por dentro), TTF_Render*, SDL_FreeSurface — passa por
 * esse contador. Aqui sao TRES fios fazendo isso: dois de decode e o
 * principal. Perde-se um incremento, o contador chega a zero com a estrutura
 * ainda em uso, SDL_FreeFormat a libera e a tira da lista, e o proximo
 * SDL_FreeSurface anda por memoria liberada. A C9 traz a SDL 2.0.4.
 *
 * E o mesmo padrao do crash sem explicacao de 03/09 ("decremento de refcount
 * seguido de free" em libSDL2, registrado na memoria como aberto).
 *
 * O CONSERTO: pedir cada formato uma vez, no fio principal, ANTES de qualquer
 * fio existir, e somar 2^28 ao refcount. Uma corrida de ++/-- desloca o
 * contador por unidades; com essa almofada ele nunca chega a zero, e
 * SDL_FreeFormat nunca libera nem remove da lista uma estrutura que outro fio
 * esta usando. Os formatos indexados (paleta) nao entram na lista — cada
 * superficie tem o seu — e ficam como estao. Serializar tudo com uma trava
 * nossa seria a alternativa, e custaria segurar o fio de desenho enquanto um
 * decode de 2 s roda; isto aqui custa zero por quadro.
 *
 * Vale em qualquer SDL: numa versao com a trava (2.0.22+) e so um refcount
 * alto que ninguem le.
 */
static SDL_INLINE void nv_blindar_formatos(void) {
  static const Uint32 fmts[] = {
    SDL_PIXELFORMAT_RGB332,   SDL_PIXELFORMAT_RGB444,   SDL_PIXELFORMAT_RGB555,
    SDL_PIXELFORMAT_BGR555,   SDL_PIXELFORMAT_ARGB4444, SDL_PIXELFORMAT_RGBA4444,
    SDL_PIXELFORMAT_ABGR4444, SDL_PIXELFORMAT_BGRA4444, SDL_PIXELFORMAT_ARGB1555,
    SDL_PIXELFORMAT_RGBA5551, SDL_PIXELFORMAT_ABGR1555, SDL_PIXELFORMAT_BGRA5551,
    SDL_PIXELFORMAT_RGB565,   SDL_PIXELFORMAT_BGR565,   SDL_PIXELFORMAT_RGB24,
    SDL_PIXELFORMAT_BGR24,    SDL_PIXELFORMAT_RGB888,   SDL_PIXELFORMAT_RGBX8888,
    SDL_PIXELFORMAT_BGR888,   SDL_PIXELFORMAT_BGRX8888, SDL_PIXELFORMAT_ARGB8888,
    SDL_PIXELFORMAT_RGBA8888, SDL_PIXELFORMAT_ABGR8888, SDL_PIXELFORMAT_BGRA8888,
    SDL_PIXELFORMAT_ARGB2101010,
  };
  size_t i;
  for (i = 0; i < sizeof fmts / sizeof fmts[0]; i++) {
    SDL_PixelFormat *f = SDL_AllocFormat(fmts[i]);
    if (f) f->refcount += 1 << 28;   /* nunca devolvido: e a almofada */
  }
}

#endif
