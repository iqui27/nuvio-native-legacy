// A EXTENSAO DE INFORMACOES DO MENU DO CARTAZ NA HOME (dono, 06/10/2026), em
// PNG, com a fonte da TV (NUVIO_SHOT_FONTE=3, Montserrat). Sem rede: o resumo
// (notas, duracao, classificacao) e semeado no cache de extras.h, os amigos
// pelo indice de amigostitulo.h e a agenda por agenda_registrar.
//
//   1. Continuar assistindo (serie em andamento): menu e extensao a DIREITA do
//      cartaz, com "T2E5 · ... · 24 min restantes", amigos e agenda;
//   2. um cartaz na ponta direita de "Em alta": os dois a ESQUERDA;
//   3. (sem GL) ctxinfo_geometria: direita, esquerda, encolhida, separada do
//      outro lado do cartaz, centrada e ao lado do painel de Salvos.
//
// Confere o que a extensao mostra (ctxinfo_texto), o lado e que nada se
// sobrepoe ao menu.
//
//   bash tests/ctxinfo_shot.sh [pasta]
#include "ajustes.h"
#include "amigostitulo.h"
#include "agenda.h"
#include "artehero.h"
#include "catalogo.h"
#include "corviva.h"
#include "ctxinfo.h"
#include "ctxmenu.h"
#include "dados.h"
#include "extras.h"
#include "fileiras.h"
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
#include <time.h>

static SDL_Window *janela;
static const char *dirDados;
static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-70s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

static void gravar(const char *png) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, png) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("  captura: %s\n", png);
}

static void quadros(int n, const char *png) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_Event e;
    while (SDL_PollEvent(&e)) { if (ctx_aberto()) ctx_evento(&e); else home_evento(&e); }
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
    if (png && i == n - 1) gravar(png);
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

// O resumo do titulo do menu no cache, como se o fio tivesse voltado da rede
// (o cache guarda 16 titulos: semeia-se o do menu aberto).
static void semear(void) {
  const CatItem *ci = ctx_titulo();
  ExResumo r;
  if (!ci) return;
  memset(&r, 0, sizeof r);
  r.cru[EX_IMDB] = 87; r.cru[EX_TOMATOES] = 970; r.cru[EX_TRAKT] = 860;
  r.duracao = strcmp(ci->tipo, "series") ? 118 : 52;
  snprintf(r.cert, sizeof r.cert, "%s", strcmp(ci->tipo, "series") ? "14" : "16");
  r.pronto = 1;
  extras_resumo_definir(ci->imdb, &r);
}

// Segura OK no cartao em foco ate o menu abrir (a home mede NV_HOLD_MS).
static void segurar(void) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; home_evento(&e);
  quadros(1, NULL); SDL_Delay(NV_HOLD_MS + 150); quadros(2, NULL);
  semear();
  e.type = SDL_KEYUP; ctx_evento(&e); home_evento(&e);
}

static void fechar(void) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e);
  quadros(30, NULL);
}

// A GEOMETRIA, pura.
static void geometria(void) {
  CtxInfoGeo g;
  GfxRect c;
  printf("\ngeometria:\n");
  c = (GfxRect){ 104, 560, 420, 236 };                 // cartaz a esquerda
  ctxinfo_geometria(&c, 554, -1, 420, &g);
  confere("cartaz a esquerda: menu e extensao juntos a direita",
          g.lado == 1 && !g.separada && g.menuX == 554 && g.infoX == 554 + 420 + CTXI_GAP && g.infoW == CTXI_W);
  c = (GfxRect){ 1600, 560, 212, 318 };                // cartaz a direita
  ctxinfo_geometria(&c, 1150, -1, 420, &g);
  confere("cartaz a direita: os dois a esquerda", g.lado == -1 && !g.separada &&
          g.menuX + 420 == 1570 && g.infoX + g.infoW == g.menuX - CTXI_GAP);
  c = (GfxRect){ 902, 85, 190, 284 };                  // apertado: encolhe
  ctxinfo_geometria(&c, 1122, -1, 420, &g);
  confere("apertado: a extensao encolhe e continua colada",
          !g.separada && g.infoW >= CTXI_W_MIN && g.infoW < CTXI_W);
  c = (GfxRect){ 720, 300, 480, 270 };                 // largo no meio: separada
  ctxinfo_geometria(&c, 1230, -1, 420, &g);
  confere("sem lugar dos dois lados: do outro lado do cartaz",
          g.separada && g.lado == -1 && g.infoX + g.infoW <= 720 - 29);
  { CtxCartaoGeo cg;
    c = (GfxRect){ 104, 400, 260, 390 };
    ctxinfo_cartao_geo(&c, 500, 420, &cg);
    confere("cartao: cresce do cartaz a esquerda, menu a direita",
            cg.lado == 1 && cg.cartao.x == 104 && cg.menuX == 104 + CTXI_CARTAO_W + CTXI_GAP &&
            cg.cartao.y == 400 && cg.cartao.h == 500);
    c = (GfxRect){ 1620, 300, 260, 390 };
    ctxinfo_cartao_geo(&c, 520, 420, &cg);
    confere("cartao: ultima coluna, cresce para a esquerda e o menu vai a esquerda",
            cg.lado == -1 && cg.cartao.x + cg.cartao.w == 1880 &&
            cg.menuX + 420 + CTXI_GAP == cg.cartao.x);
    c = (GfxRect){ 700, 900, 260, 390 };
    ctxinfo_cartao_geo(&c, 520, 420, &cg);
    confere("cartao: preso entre as margens de cima e de baixo",
            cg.cartao.y + cg.cartao.h <= 1080 - 48 + 0.5f && cg.cartao.y >= 48); }
  ctxinfo_geometria(NULL, 750, -1, 420, &g);
  confere("sem cartaz: o grupo centrado na tela",
          g.lado == 1 && g.menuX + (420 + CTXI_GAP + CTXI_W) * 0.5f > 959 &&
          g.menuX + (420 + CTXI_GAP + CTXI_W) * 0.5f < 961);
  ctxinfo_geometria(NULL, 1298, 1508, 420, &g);
  confere("painel de Salvos: menu no centro dele, extensao a esquerda",
          g.menuX == 1298 && g.lado == -1 && g.infoX + g.infoW == 1298 - CTXI_GAP);
}

static void ajustes(int vidro) {
  char cam[700];
  FILE *a;
  const char *fonte = getenv("NUVIO_SHOT_FONTE");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma 0\ntrailerHero 0\nhomeLayoutLocal 0\nvidroLocal %d\n"
             "modernLandscapePostersEnabled 1\n", vidro);
  if (fonte) fprintf(a, "fonteInterface %d\n", atoi(fonte));
  fclose(a);
  ajustes_dir(dirDados);
}

// Confere o menu aberto: extensao desenhada, do lado pedido, sem sobrepor.
static void conferir(const char *rotulo, int ladoEsperado, const char *deve) {
  GfxRect ri, rm;
  int lado = 0, tem;
  char msg[200], t[1400];
  quadros(40, NULL);
  tem = ctx_info_caixa(&ri, &rm, &lado);
  ctxinfo_texto(ctx_titulo(), NULL, t, sizeof t);
  printf("%s[extensao %.0f,%.0f %.0fx%.0f | menu %.0f,%.0f | lado %d]\n", t, ri.x, ri.y, ri.w, ri.h,
         rm.x, rm.y, lado);
  snprintf(msg, sizeof msg, "%s: o menu fica do lado %s do cartao", rotulo,
           ladoEsperado > 0 ? "direito" : "esquerdo");
  confere(msg, tem && lado == ladoEsperado);
  snprintf(msg, sizeof msg, "%s: sem sobrepor o menu, dentro da tela", rotulo);
  confere(msg, tem && (lado > 0 ? rm.x >= ri.x + ri.w - 0.5f : rm.x + rm.w <= ri.x + 0.5f) &&
               rm.x >= 0.0f && rm.x + rm.w <= 1920.5f &&
               ri.x >= 0.0f && ri.x + ri.w <= 1920.5f && ri.y >= 0.0f && ri.y + ri.h <= 1080.5f);
  snprintf(msg, sizeof msg, "%s: mostra \"%s\"", rotulo, deve);
  confere(msg, strstr(t, deve) != NULL);
}

static const char *NOMES[] = {
  "Ruptura", "A Ilha do Farol", "Mata Fechada", "Sofá no Deserto", "Preto e Branco",
  "Ensaio Seis", "Noite de Verão", "O Último Trem", "Cidade Cinza", "Rio Acima",
};
#define NN (int)(sizeof NOMES / sizeof *NOMES)

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-ctxinfo-shots";
  static CatItem itens[40];
  static CatFileira fils[2];
  SDL_GLContext gl;
  char png[800], cache[700];
  int i, k, total = 0;

  geometria();
  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-ctxinfo-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: extensao do menu", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  extras_carregar("deploy/app/art");
  ajustes(0);
  assert(home_iniciar("deploy/app/art"));

  // Duas fileiras: Continuar assistindo (series em andamento) e Em alta.
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (k = 0; k < 2; k++) {
    CatFileira *f = &fils[k];
    snprintf(f->chave, sizeof f->chave, "%s", k ? "pop_movie" : "continue_watching");
    snprintf(f->titulo, sizeof f->titulo, "%s", k ? "Em alta - Filme" : "Continuar assistindo");
    snprintf(f->tipo, sizeof f->tipo, "%s", k ? "movie" : "series");
    if (k) { snprintf(f->base, sizeof f->base, "https://addon.invalid/x");
             snprintf(f->catId, sizeof f->catId, "pop_movie"); }
    f->ini = total; f->n = k ? 12 : 5;
    for (i = 0; i < f->n; i++) {
      CatItem *c = &itens[total + i];
      int a = (total + i + 3) % 40;
      snprintf(c->imdb, sizeof c->imdb, "tt90%05d", total + i);
      snprintf(c->tipo, sizeof c->tipo, "%s", k ? "movie" : "series");
      snprintf(c->titulo, sizeof c->titulo, "%s", NOMES[(total + i) % NN]);
      snprintf(c->genero, sizeof c->genero, k ? "Filme · Drama · Mistério" : "Programa de TV · Ficção científica · Drama");
      snprintf(c->meta, sizeof c->meta, k ? "%d" : "%d · 2 temporadas", 2018 + (total + i) % 8);
      snprintf(c->sinopse, sizeof c->sinopse, "%s",
               k ? "Uma restauradora de pianos volta à cidade onde cresceu para vender a casa da mãe e encontra, "
                   "dentro de um dos instrumentos, cartas que contam outra história da família — e de quem ela é."
                 : "Funcionários de uma empresa têm as memórias do trabalho separadas cirurgicamente das da vida "
                   "pessoal. Quando um colega reaparece do lado de fora, Mark começa a desconfiar do que faz lá dentro.");
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", a);
      snprintf(c->backdropCatalogo, sizeof c->backdropCatalogo, "%s", c->backdrop);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", a);
      snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", a);
      c->nota = 70 + (total + i) % 20;
      if (!k) {
        c->progresso = 35 + i * 9; c->restanteMin = 24 + i * 3;
        c->temporada = 2; c->episodio = 5 + i;
        snprintf(c->nomeEpisodio, sizeof c->nomeEpisodio, "%s", "Que Venha o Caos");
        c->naLista = i == 0;
      }
    }
    total += f->n;
  }
  cat_definir_tudo(itens, total, fils, 2);

  agenda_definir_hoje("2026-10-06");
  agenda_registrar(itens[0].imdb, itens[0].titulo, itens[0].poster, "Returning Series",
                   2, 9, "Episódio 9", "2026-10-10", "2026-10-03");
  quadros(60, NULL);

  printf("\nContinuar assistindo:\n");
  tecla(SDLK_DOWN); quadros(90, NULL);
  // Amigos (agora, com o feed social parado: a home so remonta o indice
  // quando socialvis muda) (o indice que socialvis.h alimenta) e a agenda da primeira serie.
  { SvEvento ev[3];
    memset(ev, 0, sizeof ev);
    snprintf(ev[0].pessoaId, sizeof ev[0].pessoaId, "a1"); snprintf(ev[0].pessoaNome, sizeof ev[0].pessoaNome, "Ana Lima");
    ev[0].acao = SV_REACAO; ev[0].reacao = SV_REAC_GOSTOU;
    snprintf(ev[1].pessoaId, sizeof ev[1].pessoaId, "a2"); snprintf(ev[1].pessoaNome, sizeof ev[1].pessoaNome, "Rafa Souza");
    ev[1].acao = SV_FIM; ev[1].reacao = SV_REAC_NADA; ev[1].temporada = 2; ev[1].episodio = 8;
    for (i = 0; i < 2; i++) {
      snprintf(ev[i].imdb, sizeof ev[i].imdb, "%s", itens[0].imdb);
      snprintf(ev[i].tipo, sizeof ev[i].tipo, "series");
      snprintf(ev[i].titulo, sizeof ev[i].titulo, "%s", itens[0].titulo);
      ev[i].quando = (long long)time(NULL) - 3600 * (i + 1);
    }
    amigostitulo_montar(ev, 2); }
  segurar();
  confere("o menu abriu no cartao da fileira", ctx_aberto());
  quadros(40, NULL);
  snprintf(png, sizeof png, "%s/home-menu-extensao.png", saida);
  quadros(1, png);
  conferir("continuar", 1, "T2E5");
  { char t[1400];
    ctxinfo_texto(ctx_titulo(), NULL, t, sizeof t);
    confere("continuar: notas IMDb, RT e Trakt do resumo",
            strstr(t, " 0=860") && strstr(t, " 1=87") && strstr(t, " 3=970"));
    confere("continuar: amigos e agenda", strstr(t, "friends: A") && !strstr(t, "schedule: \n")); }
  fechar();

  printf("\nEm alta, cartaz na ponta direita:\n");
  // Abaixo de Continuar assistindo ha a fileira "Amigos assistindo".
  tecla(SDLK_DOWN); quadros(40, NULL); tecla(SDLK_DOWN); quadros(60, NULL);
  for (i = 0; i < 7; i++) { tecla(SDLK_RIGHT); quadros(20, NULL); }
  quadros(60, NULL);
  segurar();
  confere("em alta: o menu abriu", ctx_aberto());
  quadros(40, NULL);
  snprintf(png, sizeof png, "%s/home-menu-extensao-esquerda.png", saida);
  quadros(1, png);
  { HomeItem hi;
    int dir = home_item_focado(&hi) && hi.rect.x + hi.rect.w > 1920.0f - 900.0f;
    if (dir) conferir("em alta", -1, "Filme");
    else { printf("  (cartaz em foco em x=%.0f: nao ficou na direita)\n", hi.rect.x); conferir("em alta", 1, "Filme"); } }
  fechar();

  printf("\nDestaque 4:3 (info sobre a arte):\n");
  fil_definir_tipo("pop_movie", FIL_TIPO_DESTAQUE_QUADRADO);
  quadros(90, NULL);
  for (i = 0; i < 8; i++) { tecla(SDLK_LEFT); quadros(20, NULL); }
  quadros(60, NULL);
  segurar();
  confere("4:3: o menu abriu", ctx_aberto());
  quadros(60, NULL);
  snprintf(png, sizeof png, "%s/home-menu-destaque43.png", saida);
  quadros(1, png);
  fechar();

  (void)gl;
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
