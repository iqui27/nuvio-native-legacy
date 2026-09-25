// CAPTURA DO TEMA "Dinâmica" (cor viva, corviva.h) na home e na pagina de
// titulo, SEM REDE: as artes sao as de deploy/app/art, escolhidas pela cor
// que a extracao tira delas (07 = vermelho do vestido, 01 = mar turquesa, 28
// = mata verde). Duas artes de cores opostas lado a lado sao o que prova que a
// cor SEGUE o titulo, e nao so que ela mudou uma vez.
//
// O laco de quadro e o de main.c na ordem que importa: tex_bombear, as
// atualizacoes, corviva_quadro (UMA vez, antes do desenho) e so entao o
// desenho — que e quem pede a cor do quadro seguinte.
#include "ajustes.h"
#include "artehero.h"
#include "catalogo.h"
#include "corviva.h"
#include "dados.h"
#include "detail.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { const char *nome, *arte; } TITULOS[] = {
  { "O Diabo Veste Vermelho", "deploy/app/art/07.jpg" },
  { "A Ilha do Farol",        "deploy/app/art/01.jpg" },
  { "Mata Fechada",           "deploy/app/art/28.jpg" },
  { "Sofá no Deserto",        "deploy/app/art/00.jpg" },
  { "Preto e Branco",         "deploy/app/art/02.jpg" },
  { "Ensaio 6",               "deploy/app/art/05.jpg" },
};
#define NT (int)(sizeof TITULOS / sizeof *TITULOS)

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
  { float r, g, b; ajustes_acento(&r, &g, &b);
    printf("captura: %s  acento=#%02x%02x%02x fundo=#%02x%02x%02x\n", bmp,
           (int)(r * 255 + .5f), (int)(g * 255 + .5f), (int)(b * 255 + .5f),
           (int)(NV_COR_FUNDO_R * 255 + .5f), (int)(NV_COR_FUNDO_G * 255 + .5f),
           (int)(NV_COR_FUNDO_B * 255 + .5f)); }
}

static int detalhe, telaAjustes;
static void quadros(int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    tex_bombear(8);
    if (telaAjustes) ajustes_atualizar(1.0f / 60.0f, agora);
    else home_atualizar(1.0f / 60.0f, agora);
    if (detalhe) detail_atualizar(1.0f / 60.0f, agora);
    corviva_quadro(1.0f / 60.0f, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1.0f);
    if (telaAjustes) ajustes_desenhar(agora);
    else home_desenhar(agora);
    if (detalhe) detail_desenhar(agora);
    if (bmp && i == n - 1) gravar(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(16);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  if (telaAjustes) { ajustes_evento(&e); return; }
  if (detalhe) detail_evento(&e); else home_evento(&e);
  e.type = SDL_KEYUP;
  if (detalhe) detail_evento(&e); else home_evento(&e);
}

static int corDaLogo = 0;   // V_LIGA: 0 = ligada
static void tema(int t) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  // 12 Dinâmica, 13 estilizada, 14 gradiente, 15 imersiva; idioma 0 = pt.
  fprintf(a, "idioma 0\nselected_theme %d\ntrailerHero 1\ncorDaLogoLocal %d\n", t, corDaLogo);
  fclose(a);
  ajustes_dir(dirDados);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-corviva";
  static CatItem itens[NT];
  CatFileira fil;
  SDL_GLContext gl;
  char bmp[700], cache[700];
  int i;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-corviva-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: cor viva", SDL_WINDOWPOS_CENTERED,
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
  artehero_definir_falhou(tex_falhou);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  tema(12);
  assert(home_iniciar("deploy/app/art"));

  memset(itens, 0, sizeof itens);
  for (i = 0; i < NT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90000%02d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", TITULOS[i].nome);
    snprintf(c->genero, sizeof c->genero, "Filme · Drama");
    snprintf(c->meta, sizeof c->meta, "2026 · 2 h 04 min");
    snprintf(c->sinopse, sizeof c->sinopse,
             "Sinopse de enchimento, comprida o bastante para ocupar as linhas "
             "que o destaque reserva para ela, como num titulo de verdade.");
    snprintf(c->backdrop, sizeof c->backdrop, "%s", TITULOS[i].arte);
    snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
    snprintf(c->poster, sizeof c->poster, "%s", TITULOS[i].arte);
    c->nota = 80 + i;
  }
  memset(&fil, 0, sizeof fil);
  snprintf(fil.chave, sizeof fil.chave, "ensaio_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Em alta");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NT;
  cat_definir_tudo(itens, NT, &fil, 1);
  quadros(90, NULL);

  // Foco na fileira, primeiro card: o destaque e o do card focado.
  tecla(SDLK_DOWN);
  quadros(120, NULL);
  snprintf(bmp, sizeof bmp, "%s-1-home-dinamica-vermelho.bmp", saida);
  quadros(1, bmp);

  // Um para a direita: o mar. Meia transicao (~250 ms depois do assentar) e
  // a chegada.
  tecla(SDLK_RIGHT);
  quadros(120, NULL);
  snprintf(bmp, sizeof bmp, "%s-2-home-dinamica-turquesa.bmp", saida);
  quadros(1, bmp);

  // Rolagem rapida por quatro cards (um a cada 100 ms): a cor nao pode piscar
  // no meio — so a do ultimo, quando o foco para.
  { int k, r0 = corviva_retargets();
    for (k = 0; k < 3; k++) { tecla(SDLK_RIGHT); quadros(6, NULL); }
    for (k = 0; k < 3; k++) { tecla(SDLK_LEFT);  quadros(6, NULL); }
    printf("[shot] rolagem rapida: %d troca(s) de cor durante a rolagem\n",
           corviva_retargets() - r0);
    quadros(120, NULL); }

  // Estilizada, no mesmo titulo: o fundo inteiro ganha o azul-noite.
  tema(13);
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-3-home-estilizada-turquesa.bmp", saida);
  quadros(1, bmp);
  tecla(SDLK_RIGHT);            // mata verde
  quadros(120, NULL);
  snprintf(bmp, sizeof bmp, "%s-4-home-estilizada-verde.bmp", saida);
  quadros(1, bmp);

  // Pagina do titulo (estilizada): abre o focado.
  { HomeItem hi;
    assert(home_item_focado(&hi));
    detail_abrir(&hi);
    detalhe = 1;
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-5-detalhe-estilizada-verde.bmp", saida);
    quadros(1, bmp); }
  // Volta, vai ao vermelho e abre de novo.
  tecla(SDLK_ESCAPE);
  quadros(60, NULL);
  detalhe = 0;
  quadros(10, NULL);
  tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  quadros(120, NULL);
  { HomeItem hi;
    assert(home_item_focado(&hi));
    detail_abrir(&hi);
    detalhe = 1;
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-6-detalhe-estilizada-vermelho.bmp", saida);
    quadros(1, bmp); }
  // A mesma pagina com "Dinâmica" (so o destaque): o fundo volta ao #0D0D0D.
  tema(12);
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-7-detalhe-dinamica-vermelho.bmp", saida);
  quadros(1, bmp);

  // Ajustes, na linha "Cor de destaque", com a estilizada: ninguem pede cor
  // nesta tela, entao fica a do ultimo titulo (o vermelho da pagina acima).
  tema(13);
  detalhe = 0;
  telaAjustes = 1;
  ajustes_iniciar();
  quadros(30, NULL);
  tecla(SDLK_ESCAPE);                       // coluna de secoes
  for (i = 0; i < 5; i++) tecla(SDLK_DOWN); // "Interface e conta"
  tecla(SDLK_RETURN);
  for (i = 0; i < 3; i++) tecla(SDLK_DOWN); // idioma, animacoes, resolucao -> tema
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-8-ajustes-estilizada.bmp", saida);
  quadros(1, bmp);

  // --- SEGUNDA PARTE: titulos com LOGO (argv[2] = pasta com <tt>-bg.jpg e
  //     <tt>-logo.png, as do metahub). Sem a pasta, a captura para aqui.
  if (argc > 2) {
    static const struct { const char *tt, *nome; } COM_LOGO[] = {
      { "tt0264464", "Prenda-me Se For Capaz" }, { "tt1517268", "Barbie" },
      { "tt0133093", "Matrix" }, { "tt1160419", "Duna" }, { "tt4574334", "Stranger Things" },
    };
    static CatItem it2[5];
    int n2 = 5;
    telaAjustes = 0; detalhe = 0;
    memset(it2, 0, sizeof it2);
    for (i = 0; i < n2; i++) {
      CatItem *c = &it2[i];
      snprintf(c->imdb, sizeof c->imdb, "%s", COM_LOGO[i].tt);
      snprintf(c->tipo, sizeof c->tipo, "movie");
      snprintf(c->titulo, sizeof c->titulo, "%s", COM_LOGO[i].nome);
      snprintf(c->genero, sizeof c->genero, "Filme · Drama");
      snprintf(c->meta, sizeof c->meta, "2002 · 2 h 21 min");
      snprintf(c->sinopse, sizeof c->sinopse, "Sinopse de enchimento, comprida o bastante para ocupar as linhas que o destaque reserva para ela.");
      snprintf(c->backdrop, sizeof c->backdrop, "%s/%s-bg.jpg", argv[2], COM_LOGO[i].tt);
      snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
      snprintf(c->poster, sizeof c->poster, "%s", c->backdrop);
      snprintf(c->logo, sizeof c->logo, "%s/%s-logo.png", argv[2], COM_LOGO[i].tt);
      c->nota = 81;
    }
    snprintf(fil.chave, sizeof fil.chave, "ensaio_logo");
    fil.ini = 0; fil.n = n2;
    cat_definir_tudo(it2, n2, &fil, 1);
    tema(14);
    quadros(120, NULL);
    tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_DOWN);   // primeiro card
    { int k; for (k = 0; k < 6; k++) tecla(SDLK_LEFT); }
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-9-home-gradiente-catchme.bmp", saida);
    quadros(1, bmp);
    { HomeItem hi;
      assert(home_item_focado(&hi));
      detail_abrir(&hi); detalhe = 1;
      quadros(150, NULL);
      snprintf(bmp, sizeof bmp, "%s-10-detalhe-gradiente-catchme.bmp", saida);
      quadros(1, bmp);
      corDaLogo = 1; tema(14);   // cor da logo DESLIGADA: volta a arte
      quadros(90, NULL);
      snprintf(bmp, sizeof bmp, "%s-11-detalhe-gradiente-catchme-sem-logo.bmp", saida);
      quadros(1, bmp);
      corDaLogo = 0; tema(15);   // imersiva
      quadros(120, NULL);
      snprintf(bmp, sizeof bmp, "%s-12-detalhe-imersiva-catchme.bmp", saida);
      quadros(1, bmp); }
    tecla(SDLK_ESCAPE);
    quadros(60, NULL);
    detalhe = 0;
    quadros(30, NULL);
    snprintf(bmp, sizeof bmp, "%s-13-home-imersiva-catchme.bmp", saida);
    quadros(1, bmp);
    tecla(SDLK_RIGHT);          // Barbie
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-14-home-imersiva-barbie.bmp", saida);
    quadros(1, bmp);
    tecla(SDLK_RIGHT);          // Matrix
    quadros(150, NULL);
    snprintf(bmp, sizeof bmp, "%s-15-home-imersiva-matrix.bmp", saida);
    quadros(1, bmp);
    { HomeItem hi;
      assert(home_item_focado(&hi));
      detail_abrir(&hi); detalhe = 1;
      quadros(150, NULL);
      snprintf(bmp, sizeof bmp, "%s-16-detalhe-imersiva-matrix.bmp", saida);
      quadros(1, bmp); }
    tecla(SDLK_ESCAPE); quadros(60, NULL); detalhe = 0; quadros(10, NULL);
    tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);   // Stranger Things
    quadros(150, NULL);
    { HomeItem hi;
      assert(home_item_focado(&hi));
      detail_abrir(&hi); detalhe = 1;
      quadros(150, NULL);
      snprintf(bmp, sizeof bmp, "%s-17-detalhe-imersiva-stranger.bmp", saida);
      quadros(1, bmp); }
  }

  corviva_gravar_se_preciso(1);
  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
