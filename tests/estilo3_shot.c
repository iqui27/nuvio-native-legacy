// O MENU CONTEXTUAL DE ESTILO, AO LADO DA FILEIRA (dono, 14/10: "tem que ser um
// menu contextual, nao precisa ser gigante, pode ser minimalista ao lado"),
// SEM REDE (artes de deploy/app/art). Alem dos PNG, o teste confere o contrato:
// a ilha e pequena, fica ao lado do cartao em foco, a fileira muda ao vivo e
// o Voltar devolve a forma de antes.
//
//   <pref>-1-pessoas-redonda  fileira de pessoas em Redonda, menu ao lado
//   <pref>-2-cartaz-pmg       fileira de cartazes, P/M/G na linha em foco
//   <pref>-3-ordem            grupo de colecao, linha Ordem em foco
//   <pref>-4-borda-direita    menu aberto no ultimo cartao: vira para a esquerda
//
//   bash tests/estilo3_shot.sh /tmp/nv-estilo3 [prefixo]
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
    ctx_atualizar(1.0f / 60.0f, agora);
    ctx_desenhar(agora);
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

static void teclaCtx(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  ctx_evento(&e);
  e.type = SDL_KEYUP;
  ctx_evento(&e);
  quadros(6, NULL);
}

static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 0\nhomeLayoutLocal 0\nvidroLocal 1\n"
             "modernLandscapePostersEnabled 1\nselected_theme 0\n");
  fclose(a);
  ajustes_dir(dirDados);
  artehero_fundo_addon(ajustes_fundo_addon());
  posterprov_preferir_addon(ajustes_poster_addon());
  col_arte_conta(ajustes_col_arte_conta());
  ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC=60|92: low/high glass opacity
}

static const char *JS =
  "{\"collections\":[{\"id\":\"cs\",\"title\":\"Streaming\","
  "\"backdropImageUrl\":\"deploy/app/art/07.jpg\",\"folders\":["
  "{\"id\":\"a\",\"title\":\"Com capa\",\"coverImageUrl\":\"deploy/app/art/03.jpg\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]},"
  "{\"id\":\"b\",\"title\":\"So capa\",\"coverImageUrl\":\"deploy/app/art/05.jpg\",\"tileShape\":\"LANDSCAPE\","
  "\"sources\":[{\"addonBaseUrl\":\"https://addon.invalid/x\",\"type\":\"movie\",\"catalogId\":\"k\"}]}]}]}";

#define PESSOAS "tmdb.people_popular"
static const char *NOMES[] = { "Greta Gerwig", "Denis Villeneuve", "Bong Joon-ho",
  "Ava DuVernay", "Wes Anderson", "Martin Scorsese", "Chloé Zhao", "Jordan Peele" };

static void focaFileira(const char *titulo) {
  int i;
  for (i = 0; i < 14; i++) teclaHome(SDLK_UP);
  for (i = 0; i < 12 && !strstr(home_rastro_foco(), titulo); i++) {
    teclaHome(SDLK_DOWN);
    quadros(4, NULL);
  }
  assert(strstr(home_rastro_foco(), titulo));
  quadros(30, NULL);   // a fileira assenta antes de o menu nascer ao lado dela
}

// O menu aberto esta inteiro na tela, e e pequeno, e nao cobre o cartao que a
// home desenha em foco AGORA (nao o de quando abriu).
static void confereAoLado(void) {
  GfxRect m, a;
  assert(ctx_estilo_rect(&m));
  assert(home_ancora_foco(&a));
  assert(m.w * m.h < 0.30f * 1920.0f * 1080.0f);
  assert(m.x >= 0 && m.x + m.w <= 1920.0f && m.y >= 0 && m.y + m.h <= 1080.0f);
  assert(m.x >= a.x + a.w || m.x + m.w <= a.x);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-estilo3/est");
  static CatItem itens[20];
  static CatFileira fils[3];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, k;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-estilo3-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: estilo da fileira", SDL_WINDOWPOS_CENTERED,
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
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Continuar assistindo");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  fils[0].ini = 0; fils[0].n = 4;
  snprintf(fils[1].chave, sizeof fils[1].chave, PESSOAS);
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Diretores populares");
  snprintf(fils[1].tipo, sizeof fils[1].tipo, "movie");
  snprintf(fils[1].base, sizeof fils[1].base, "https://addon.invalid/x");
  snprintf(fils[1].catId, sizeof fils[1].catId, "people_popular");
  fils[1].ini = 4; fils[1].n = 8;
  snprintf(fils[2].chave, sizeof fils[2].chave, "tmdb.trending_movie");
  snprintf(fils[2].titulo, sizeof fils[2].titulo, "Em alta");
  snprintf(fils[2].tipo, sizeof fils[2].tipo, "movie");
  snprintf(fils[2].base, sizeof fils[2].base, "https://addon.invalid/x");
  snprintf(fils[2].catId, sizeof fils[2].catId, "trending");
  fils[2].ini = 12; fils[2].n = 8;
  for (i = 0; i < 20; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90%03d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    if (i < 4) {
      snprintf(c->titulo, sizeof c->titulo, "Titulo Fixture %d", i);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", i);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", i);
      c->progresso = 15 + i * 20;
    } else if (i >= 12) {
      snprintf(c->titulo, sizeof c->titulo, "Em alta %d", i - 12);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", i - 8);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", i - 8);
    } else {
      snprintf(c->titulo, sizeof c->titulo, "%s", NOMES[i - 4]);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/elenco/%02d_%d.jpg", (i - 4) / 6, (i - 4) % 6);
      snprintf(c->backdrop, sizeof c->backdrop, "%s", c->poster);
    }
  }
  assert(col_definir_json(JS) == 2);
  cat_definir_tudo(itens, 20, fils, 3);
  quadros(60, NULL);

  // 1. Pessoas: o menu abre ao lado do cartao em foco, pequeno, e a fileira
  // vira Redonda ao vivo; o Voltar devolve o Pôster.
  { GfxRect m, ancora;
    int antes;
    focaFileira("Diretores populares");
    antes = fil_tipo(PESSOAS);
    ctx_abrir_fileira(PESSOAS, "Diretores populares");
    quadros(30, NULL);
    assert(ctx_estilo_rect(&m));
    assert(home_ancora_foco(&ancora));
    // Pequeno: menos de 30% da tela, e nao o modal de tela cheia.
    assert(m.w * m.h < 0.30f * 1920.0f * 1080.0f);
    // Ao lado: sem sobrepor o cartao em foco, e colado nele.
    assert(m.x >= ancora.x + ancora.w || m.x + m.w <= ancora.x);
    assert(m.y < ancora.y + ancora.h && m.y + m.h > ancora.y);
    { FilEstiloLinha l[FIL_TIPO_N];
      int n = fil_estilo_linhas(PESSOAS, l, FIL_TIPO_N), alvo = -1;
      for (k = 0; k < n; k++) if (l[k].n == 1 && l[k].tipos[0] == FIL_TIPO_REDONDA) alvo = k;
      assert(alvo >= 0);
      for (k = 0; k < alvo; k++) teclaCtx(SDLK_DOWN); }
    quadros(60, NULL);
    assert(fil_tipo(PESSOAS) == FIL_TIPO_REDONDA);   // ao vivo, antes do OK
    confereAoLado();
    snprintf(bmp, sizeof bmp, "%s-1-pessoas-redonda.bmp", saida);
    quadros(1, bmp);
    { SDL_Event e; memset(&e, 0, sizeof e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e); }
    quadros(40, NULL);
    assert(!ctx_aberto());
    assert(fil_tipo(PESSOAS) == antes);              // Voltar restaura
  }

  // 2. Cartazes: o P/M/G aparece na linha em foco; direita muda o tamanho.
  focaFileira("Em alta");
  ctx_abrir_fileira("tmdb.trending_movie", "Em alta");
  quadros(30, NULL);
  { FilEstiloLinha l[FIL_TIPO_N];
    int n = fil_estilo_linhas("tmdb.trending_movie", l, FIL_TIPO_N), alvo = -1;
    for (k = 0; k < n; k++) if (l[k].n > 1) { alvo = k; break; }
    assert(alvo >= 0);
    for (k = 0; k < alvo; k++) teclaCtx(SDLK_DOWN);
    teclaCtx(SDLK_RIGHT); }
  quadros(60, NULL);
  confereAoLado();
  snprintf(bmp, sizeof bmp, "%s-2-cartaz-pmg.bmp", saida);
  quadros(1, bmp);
  { SDL_Event e; memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e); }
  quadros(40, NULL);

  // 3. A ordem do grupo de colecao, linha no fim da lista.
  focaFileira("Streaming");
  ctx_abrir_fileira("collection_cs", "Streaming");
  quadros(30, NULL);
  for (k = 0; k < FIL_TIPO_N; k++) teclaCtx(SDLK_DOWN);
  teclaCtx(SDLK_RIGHT);
  quadros(60, NULL);
  { GfxRect m;
    assert(ctx_estilo_rect(&m));
    assert(m.w * m.h < 0.30f * 1920.0f * 1080.0f); }
  confereAoLado();
  snprintf(bmp, sizeof bmp, "%s-3-ordem.bmp", saida);
  quadros(1, bmp);
  { SDL_Event e; memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e); }
  quadros(40, NULL);

  // 4. Cartao do canto direito: o menu vira para a esquerda, dentro da tela.
  focaFileira("Em alta");
  for (k = 0; k < 6; k++) { teclaHome(SDLK_RIGHT); quadros(10, NULL); }
  quadros(40, NULL);
  ctx_abrir_fileira("tmdb.trending_movie", "Em alta");
  quadros(40, NULL);
  { GfxRect m, ancora;
    assert(ctx_estilo_rect(&m));
    assert(home_ancora_foco(&ancora));
    assert(m.x >= 0 && m.x + m.w <= 1920.0f && m.y >= 0 && m.y + m.h <= 1080.0f);
    assert(m.x >= ancora.x + ancora.w || m.x + m.w <= ancora.x); }
  confereAoLado();
  snprintf(bmp, sizeof bmp, "%s-4-borda-direita.bmp", saida);
  quadros(1, bmp);
  printf("[shot] ok\n");
  SDL_Quit();
  return 0;
}
