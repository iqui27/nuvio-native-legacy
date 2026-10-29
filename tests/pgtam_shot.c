// PÔSTER GRANDE EM 3 TAMANHOS (dono: "esse estilo de fileira pode ter 3
// tamanhos tb"), SEM REDE. Uma fileira "Top Rated - TV Show" no estilo do
// cartaz com a ficha na base (logo, "Série · Gênero · Gênero", sinopse), uma
// captura por tamanho. Os tres cartoes visiveis cobrem os casos de texto: o
// 1o e o da foto do dono (logo + genero curto), o 2o tem genero comprido e selo
// de classificacao (o genero tem de ceder e ganhar reticencias, nunca empurrar
// o selo para fora), o 3o nao tem logo (o titulo em texto ocupa a caixa).
//
// O teste acha o P/M/G pela LINHA do modal (fil_estilo_linhas), sem citar o
// enum: sem a familia de tamanhos ele falha na asserção, nao no compilador.
//
//   bash tests/pgtam_shot.sh /Volumes/ExternalSSD/tmp/fichatam [prefixo]
//     <pref>-p.png <pref>-m.png <pref>-g.png
#include "ajustes.h"
#include "nvtmp.h"
#include "addons.h"
#include "artehero.h"
#include "catalogo.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include "posterprov.h"
#include <string.h>
#include <assert.h>

#define CHAVE "tmdb.top_rated_series"

static SDL_Window *janela;
static const char *dirDados;

static void gravar(const char *bmp) {
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
}

static void quadros(int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    tex_bombear(8);
    home_atualizar(1.0f / 60.0f, agora);
    corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    gfx_ambiente_preparar();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1.0f);
    home_desenhar(agora);
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}

static void teclaHome(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 1\nhomeLayoutLocal 0\nvidroLocal 1\n"
             "modernLandscapePostersEnabled 1\nselected_theme 0\n");
  if (getenv("PGTAM_SEM_GENERO")) fprintf(a, "heroGeneroLocal 1\n");   // "Mostrar genero no destaque" desligado
  fclose(a);
  ajustes_dir(dirDados);
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
}

static void foca(const char *titulo) {
  int i;
  for (i = 0; i < 14; i++) teclaHome(SDLK_UP);
  for (i = 0; i < 12 && !strstr(home_rastro_foco(), titulo); i++) {
    teclaHome(SDLK_DOWN);
    quadros(4, NULL);
  }
  assert(strstr(home_rastro_foco(), titulo));
  quadros(90, NULL);   // a mola assenta
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-pgtam/pg");
  static CatItem itens[12];
  static CatFileira fils[2];
  FilEstiloLinha l[FIL_TIPO_N];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, nl, linha = -1;
  float ult[3][2];
  static const char *SUF[3] = { "p", "m", "g" };

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-pgtam-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: poster grande P/M/G", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_borrao_iniciar(480, 270);
  artehero_definir_falhou(tex_falhou);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajusta();
  assert(home_iniciar("deploy/app/art"));

  { AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
    assert(addons_definir_lista(&addon, 1)); }
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  snprintf(fils[0].chave, sizeof fils[0].chave, "tmdb.trending_movie");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Em alta");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  snprintf(fils[0].base, sizeof fils[0].base, "https://addon.invalid/x");
  snprintf(fils[0].catId, sizeof fils[0].catId, "trending");
  fils[0].ini = 0; fils[0].n = 4;
  snprintf(fils[1].chave, sizeof fils[1].chave, CHAVE);
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Top Rated");
  snprintf(fils[1].tipo, sizeof fils[1].tipo, "series");
  snprintf(fils[1].base, sizeof fils[1].base, "https://addon.invalid/x");
  snprintf(fils[1].catId, sizeof fils[1].catId, "top_rated");
  fils[1].ini = 4; fils[1].n = 8;
  for (i = 0; i < 12; i++) {
    CatItem *c = &itens[i];
    int a = i % 10;
    snprintf(c->imdb, sizeof c->imdb, "tt91%03d", i);
    snprintf(c->tipo, sizeof c->tipo, i < 4 ? "movie" : "series");
    snprintf(c->titulo, sizeof c->titulo, "Titulo Fixture %d", i);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", a);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", a);
    snprintf(c->genero, sizeof c->genero, "TV Show · Drama · Family");
    snprintf(c->meta, sizeof c->meta, "%d", 2018 + i);
    snprintf(c->sinopse, sizeof c->sinopse,
             "A coming-of-age story about an outsider who, against every expectation, "
             "turns a quiet farmhouse into the loudest and warmest home on the island.");
    if (a < 10) snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", a);
    c->nota = 80;
  }
  // 2o cartao da fileira: genero comprido + selo. 3o: sem logo.
  snprintf(itens[5].genero, sizeof itens[5].genero,
           "TV Show · Action & Adventure · Science Fiction & Fantasy · Drama");
  snprintf(itens[5].classificacao, sizeof itens[5].classificacao, "TV-MA");
  itens[6].logo[0] = 0;
  snprintf(itens[6].titulo, sizeof itens[6].titulo, "A Very Long Title That Must Never Overflow The Card");
  cat_definir_tudo(itens, 12, fils, 2);
  quadros(60, NULL);

  // A LINHA DO MODAL: uma so, com P/M/G, e nela o tipo que vale agora (M).
  nl = fil_estilo_linhas(CHAVE, l, FIL_TIPO_N);
  for (k = 0; k < nl; k++)
    for (i = 0; i < l[k].n; i++)
      if (l[k].tipos[i] == FIL_TIPO_POSTER_GRANDE) linha = k;
  assert(linha >= 0);
  assert(l[linha].n == 3);                      // P / M / G numa linha so
  assert(l[linha].tipos[1] == FIL_TIPO_POSTER_GRANDE);   // o M e o de sempre

  for (k = 0; k < 3; k++) {
    GfxRect r;
    assert(fil_definir_tipo(CHAVE, l[linha].tipos[k]));
    quadros(20, NULL);
    foca("Top Rated");
    assert(home_ancora_foco(&r));
    ult[k][0] = r.w; ult[k][1] = r.h;
    assert(r.x >= 0 && r.x + r.w <= 1920.0f && r.y >= 0 && r.y + r.h <= 1080.0f);
    snprintf(bmp, sizeof bmp, "%s-%s.bmp", saida, SUF[k]);
    quadros(1, bmp);
    printf("[pgtam] %s: %.0fx%.0f\n", SUF[k], r.w, r.h);
  }
  // A GEOMETRIA CRESCE: menor < medio < maior, em largura e em area.
  assert(ult[0][0] < ult[1][0] && ult[1][0] < ult[2][0]);
  assert(ult[0][1] < ult[1][1] && ult[1][1] <= ult[2][1] + 0.5f);
  assert(ult[0][0] * ult[0][1] < ult[1][0] * ult[1][1] &&
         ult[1][0] * ult[1][1] < ult[2][0] * ult[2][1]);
  printf("[pgtam] ok\n");
  SDL_Quit();
  return 0;
}
