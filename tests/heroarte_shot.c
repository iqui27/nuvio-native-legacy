// CAPTURA DA HOME POR COMBINACAO DE AJUSTE DE ARTE (22/09).
//
// Pedido do dono: "a settings de selecionar o source das artes nao ta
// funcionando, e tem que ter um toggle de ter uma versao diferente do que
// mostra no card do que ta na hero". Esta captura e a prova visual das duas
// coisas: para cada fonte (Automatico, TMDB, Trakt) com "Destaque com outra
// arte" desligado e ligado, a home com o destaque e a fileira de cards
// deitados. Desligado, card focado e destaque sao a MESMA foto; ligado, o
// card fica com a do catalogo e o destaque muda.
//
// Os itens sao do Cinemeta como chegam de verdade (background = metahub pelo
// id, e so) — exatamente o caso em que o ajuste antigo dava cinco vezes a
// mesma url. Os ajustes entram pelo ajustes.txt da pasta temporaria, o mesmo
// caminho que a TV le.
#include "app.h"
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "dados.h"
#include "descoberta.h"
#include "gfx.h"
#include "home.h"
#include "nuvem.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *tt, *nome; } TITULOS[] = {
  { "tt0111161", "The Shawshank Redemption" },
  { "tt0068646", "The Godfather" },
  { "tt0468569", "The Dark Knight" },
  { "tt1375666", "Inception" },
  { "tt0816692", "Interstellar" },
  { "tt0133093", "The Matrix" },
  { "tt0110912", "Pulp Fiction" },
  { "tt0109830", "Forrest Gump" },
};
#define NT (int)(sizeof TITULOS / sizeof *TITULOS)

static void quadros(SDL_Window *w, int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    home_atualizar(1.0f / 60.0f, SDL_GetTicks());
    home_desenhar(SDL_GetTicks());
    if (bmp && i == n - 1) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      int y;
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, bmp) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("captura: %s\n", bmp);
    }
    SDL_GL_SwapWindow(w);
    SDL_Delay(16);
  }
}

int main(int argc, char **argv) {
  const char *dir = argv[1];
  const char *saida = argc > 2 ? argv[2] : "/tmp/nuvio-heroarte";
  static const int FONTES[] = { ARTEHERO_AUTO, ARTEHERO_TMDB, ARTEHERO_TRAKT };
  static const char *NOME[] = { "auto", "", "", "tmdb", "trakt" };
  static CatItem itens[NT];
  CatFileira fil;
  SDL_Window *w;
  SDL_GLContext gl;
  char cache[600], bmp[700];
  int f, d, i;

  dados_iniciar("deploy/app/art");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: arte do destaque", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  artehero_definir_falhou(tex_falhou);
  nuvem_configurar("deploy/app/art");
  desc_tmdb("deploy/app/art");
  snprintf(cache, sizeof cache, "%s/cache", dir);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajustes_dir(dir);
  assert(home_iniciar("deploy/app/art"));

  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "%s", TITULOS[i].tt);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", TITULOS[i].nome);
    snprintf(c->genero, sizeof c->genero, "Filme");
    snprintf(c->backdrop, sizeof c->backdrop,
             "https://images.metahub.space/background/medium/%s/img", c->imdb);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster,
             "https://images.metahub.space/poster/medium/%s/img", c->imdb);
    snprintf(c->logo, sizeof c->logo,
             "https://images.metahub.space/logo/medium/%s/img", c->imdb);
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "cinemeta_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Populares - Filme");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NT;
  cat_definir_tudo(itens, NT, &fil, 1);
  quadros(w, 60, NULL);
  // FOCO NO PRIMEIRO CARD: com o foco na fileira, o fundo de tela cheia e o do
  // card focado — card e destaque do MESMO titulo lado a lado, que e o que
  // se quer comparar. O destaque do topo gira sozinho e nao serviria.
  // Terceiro card (The Dark Knight): as tres fontes tem fotos DIFERENTES
  // dele (metahub: o morcego em chamas; TMDB: Batman na frente do predio;
  // Trakt: o Coringa). O primeiro (Shawshank) tem a mesma foto nas tres e nao
  // provaria nada.
  { static const SDL_Keycode TECLAS[] = { SDLK_DOWN, SDLK_RIGHT, SDLK_RIGHT };
    int k;
    for (k = 0; k < 3; k++) {
      SDL_Event e;
      memset(&e, 0, sizeof e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = TECLAS[k];
      home_evento(&e);
      e.type = SDL_KEYUP;
      home_evento(&e);
      quadros(w, 20, NULL);
    } }

  for (f = 0; f < 3; f++)
    for (d = 0; d < 2; d++) {
      char cam[700];
      FILE *a;
      snprintf(cam, sizeof cam, "%s/ajustes.txt", dir);
      a = fopen(cam, "w");
      assert(a);
      // V_LIGA: 0 = Ligado, 1 = Desligado.
      fprintf(a, "heroFundoLocal %d\nheroDifferentFromCard %d\ntrailerHero 1\n",
              FONTES[f], d ? 0 : 1);
      fclose(a);
      ajustes_dir(dir);
      printf("[shot] fonte=%s outra=%d\n", NOME[FONTES[f]], d);
      // 12 s de quadros: metahub + /find do TMDB + busca do Trakt + decode.
      snprintf(bmp, sizeof bmp, "%s-%s-%s.bmp", saida, NOME[FONTES[f]],
               d ? "outra" : "mesma");
      quadros(w, 720, bmp);
    }

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
