// CAPTURA DA FILEIRA DE COLECAO EM CADA ESTILO (#205 "todos os styles"),
// SEM REDE (artes de deploy/app/art). Base: tests/homelayouts_shot.c (mesma
// montagem da Home, reduzida a uma retomada e um grupo de colecao).
//
// Para cada estilo do catalogo (fil_estilos da fileira de colecao): escolhe
// (fil_definir_tipo), poe o foco na fileira e fotografa. A prova visual de
// que a pasta e desenhada no card DO ESTILO — pôster, paisagens, faixa, 4:3,
// ranking, linha do tempo, ficha, palco, redonda.
//
//   bash tests/colestilos_shot.sh /tmp/nv-colestilos
#include "ajustes.h"
#include "nvtmp.h"
#include "addons.h"
#include "agenda.h"
#include "artehero.h"
#include "catalogo.h"
#include "colecoes.h"
#include "corviva.h"
#include "ctxmenu.h"
#include "dados.h"
#include "fileiras.h"
#include "gfx.h"
#include "home.h"
#include "detail.h"
#include "anim.h"
#include "layout.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include "posterprov.h"
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <math.h>

static SDL_Window *janela;
static const char *dirDados;
static float lumaLinha[1080];  // luma media do fundo por linha, do ultimo gravar

static void gravar(const char *bmp) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  // VEU DO PALCO: na faixa x 1350..1740 so ha fundo (cards acabam antes, o
  // contador fica depois); guarda a luma media de cada linha da tela.
  for (y = 0; y < 1080; y++) {
    float a = 0.0f;
    int x;
    for (x = 1350; x < 1740; x++) {
      const unsigned char *p = pix + ((1079 - y) * 1920 + x) * 4;
      a += 0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2];
    }
    lumaLinha[y] = a / 390.0f;
  }
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
    ctx_atualizar(1.0f / 60.0f, agora);
    ctx_desenhar(agora);
    // NV_HOLOFOTE=1: o holofote do Palco por cima de tudo, como app.c.
    if (getenv("NV_HOLOFOTE") && !ctx_aberto()) home_holofote_desenhar(agora);
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(8);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

// Ajustes pelo mesmo arquivo que a TV le. Idioma 0 = pt; animacoes NORMAIS.
static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fprintf(a, "idioma 0\nhomeLayoutLocal 0\nvidroLocal 1\n"
             "modernLandscapePostersEnabled 1\nselected_theme 0\n");
  fclose(a);
  ajustes_dir(dirDados);
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
}

// A fileira da conta: quatro pastas com as cadeias de arte do homelayouts.
static const char *JS =
  "{\"collections\":[{\"id\":\"cs\",\"title\":\"Streaming\","
  "\"backdropImageUrl\":\"deploy/app/art/07.jpg\",\"folders\":["
  "{\"id\":\"a\",\"title\":\"Com hero e capa\",\"heroBackdropUrl\":\"deploy/app/art/03.jpg\","
  "\"coverImageUrl\":\"deploy/app/art/poster/12.jpg\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
  "{\"id\":\"b\",\"title\":\"So capa\",\"coverImageUrl\":\"deploy/app/art/05.jpg\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
  "{\"id\":\"c\",\"title\":\"So fundo da colecao\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
  "{\"id\":\"d\",\"title\":\"Com hero sem capa\",\"heroBackdropUrl\":\"deploy/app/art/09.jpg\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}";

static const struct { int t; const char *slug; } ESTILO[] = {
  { FIL_TIPO_AUTO, "01-auto" },
  { FIL_TIPO_CARTAZ, "02-poster" },
  { FIL_TIPO_SERVICO, "03-paisagem-p" },
  { FIL_TIPO_COLECAO, "04-paisagem-m" },
  { FIL_TIPO_DESTAQUE, "05-paisagem-g" },
  { FIL_TIPO_LARGA, "06-faixa" },
  { FIL_TIPO_DESTAQUE_QUADRADO, "07-4x3" },
  { FIL_TIPO_DESTAQUE_QUADRADO_M, "08-4x3-m" },
  { FIL_TIPO_DESTAQUE_QUADRADO_G, "09-4x3-g" },
  { FIL_TIPO_RANKING, "10-ranking" },
  { FIL_TIPO_TOP10, "11-top10" },
  { FIL_TIPO_LINHA_TEMPO, "12-linha-tempo" },
  { FIL_TIPO_FICHA, "13-ficha" },
  { FIL_TIPO_PALCO, "14-palco" },
  { FIL_TIPO_REDONDA, "15-redonda" },
};
#define NE (int)(sizeof ESTILO / sizeof *ESTILO)

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-colestilos/h");
  static CatItem itens[4];
  static CatFileira fils[2];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k, nEst;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-colestilos-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: estilos da fileira de colecao", SDL_WINDOWPOS_CENTERED,
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

  // A Home exige um addon ligado mesmo quando o catalogo e um fixture local.
  { AddonRemoto addon = { .nome = "Fixture", .url = "http://127.0.0.1:1/manifest.json", .ativo = 1 };
    assert(addons_definir_lista(&addon, 1)); }
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Continuar assistindo");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  fils[0].ini = 0; fils[0].n = 4;
  // Fileira de catalogo comum: a do veu do Palco (TCL, "Latest Horror").
  fils[1] = fils[0];
  snprintf(fils[1].chave, sizeof fils[1].chave, "pop_movie");
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Em alta");
  for (i = 0; i < 4; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90%03d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Titulo Fixture %d", i);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", i);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", i);
    c->progresso = 15 + i * 20;
    // Ficha de verdade: a Ficha e o Palco mostram genero, selos e sinopse.
    snprintf(c->meta, sizeof c->meta, "202%d · 1%d2 min", i, i);
    snprintf(c->genero, sizeof c->genero, "Drama · Suspense · Mistério");
    snprintf(c->classificacao, sizeof c->classificacao, "14");
    snprintf(c->sinopse, sizeof c->sinopse, "Sinopse de enchimento, comprida o bastante para ocupar as "
             "linhas que a ficha reserva para ela, como num titulo de verdade, e mais um pouco.");
    c->nota = 68 + i;
  }
  assert(col_definir_json(JS) == 4);
  cat_definir_tudo(itens, 4, fils, 2);
  quadros(60, NULL);

  // O MESMO CONJUNTO DE ESTILOS do catalogo e o que a colecao aceita.
  { int t[FIL_TIPO_N]; const char *r[FIL_TIPO_N];
    nEst = fil_estilos("collection_cs", t, r, FIL_TIPO_N);
    assert(nEst > 0); }

  // Desce ate a fileira "Streaming" (o grupo de colecao), onde estiver ela.
  for (i = 0; i < 14; i++) tecla(SDLK_UP);
  for (i = 0; i < 12 && !strstr(home_rastro_foco(), "Streaming"); i++) {
    tecla(SDLK_DOWN);
    quadros(4, NULL);
  }
  assert(strstr(home_rastro_foco(), "Streaming"));

  for (k = 0; k < NE; k++) {
    char nome[800];
    if (!fil_definir_tipo("collection_cs", ESTILO[k].t)) continue;
    quadros(30, NULL);
    // Foco na fileira da colecao, que a remontagem devolve de onde estava.
    for (i = 0; i < 14; i++) tecla(SDLK_UP);
    for (i = 0; i < 12 && !strstr(home_rastro_foco(), "Streaming"); i++) {
      tecla(SDLK_DOWN);
      quadros(4, NULL);
    }
    assert(strstr(home_rastro_foco(), "Streaming"));
    quadros(90, NULL);
    snprintf(nome, sizeof nome, "%s-%s.bmp", saida, ESTILO[k].slug);
    quadros(1, nome);
    printf("[shot] %s: %s\n", ESTILO[k].slug, fil_tipo_rotulo(ESTILO[k].t));
  }
  // VEU DO PALCO (#205): fileira de catalogo em Poster e em Palco, em foco.
  // O Palco usa o mesmo degrade das outras: nenhuma borda dura no fundo.
  static float lumaPoster[1080];
  for (k = 0; k < 2; k++) {
    char nome[800];
    assert(fil_definir_tipo("pop_movie", k ? FIL_TIPO_PALCO : FIL_TIPO_CARTAZ));
    quadros(30, NULL);
    for (i = 0; i < 14; i++) tecla(SDLK_UP);
    for (i = 0; i < 12 && !strstr(home_rastro_foco(), "Em alta"); i++) {
      tecla(SDLK_DOWN);
      quadros(4, NULL);
    }
    assert(strstr(home_rastro_foco(), "Em alta"));
    quadros(90, NULL);
    snprintf(nome, sizeof nome, "%s-cat-%s.bmp", saida, k ? "palco" : "poster");
    quadros(1, nome);
    if (!k) memcpy(lumaPoster, lumaLinha, sizeof lumaPoster);
    else {
      float pior = 0.0f;
      int yPior = 0;
      for (i = 0; i < 1080; i++)
        if (fabsf(lumaLinha[i] - lumaPoster[i]) > pior) { pior = fabsf(lumaLinha[i] - lumaPoster[i]); yPior = i; }
      printf("[shot] cat-palco: fundo difere do Poster em ate %.1f (y=%d)\n", pior, yPior);
      assert(getenv("NV_HOLOFOTE") || pior < 8.0f);   // o veu escurece de proposito
    }
  }
  SDL_Quit();
  return 0;
}
