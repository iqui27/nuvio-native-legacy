// ANIMACAO DE ABERTURA DO "Estilo da fileira" (menu do titulo -> seletor), em PNG.
// O seletor nasce em alfa 0 e sobe por exponencial de saida (~200 ms); o quadro
// final tem de ser o de sempre. Com -DNV_ESTILO_BASE (ctxmenu.c sem a animacao) o teste
// de progresso falha.
//
//   bash tests/estiloanim_shot.sh [pasta]
#include "ajustes.h"
#include "nvtmp.h"
#include "amigostitulo.h"
#include "agenda.h"
#include "artehero.h"
#include "catalogo.h"
#include "corviva.h"
#include "ctxinfo.h"
#include "ctxmenu.h"
#ifdef NV_ESTILO_BASE
float ctx_estilo_t(void) { return 1.0f; }
#endif
#include "dados.h"
#include "extras.h"
#include "fileiras.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "ponteiro.h"
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

// Soma de brilho (R+G+B) de um retangulo do quadro atual.
static long brilho(GfxRect r) {
  static unsigned char pix[1920 * 1080 * 4];
  long soma = 0;
  int x, y, x0 = (int)r.x, y0 = (int)r.y;
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = y0; y < y0 + (int)r.h && y < 1080; y++)
    for (x = x0; x < x0 + (int)r.w && x < 1920; x++) {
      unsigned char *p = pix + ((1079 - y) * 1920 + x) * 4;
      soma += p[0] + p[1] + p[2];
    }
  return soma;
}

static GfxRect medir;   // se w > 0, quadros() mede o brilho dele antes do swap
static long medido;

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
    ponteiro_quadro(agora);
    gfx_ambiente_preparar();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    gfx_ambiente(1.0f);
    home_desenhar(agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    ctx_desenhar(agora);
    if (medir.w > 0.0f) medido = brilho(medir);
    if (png && i == n - 1) gravar(png);
    ponteiro_desenhar();
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
static const char *NOMES[] = {
  "Ruptura", "A Ilha do Farol", "Mata Fechada", "Sofá no Deserto", "Preto e Branco",
  "Ensaio Seis", "Noite de Verão", "O Último Trem", "Cidade Cinza", "Rio Acima",
};
#define NN (int)(sizeof NOMES / sizeof *NOMES)

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-ctxinfo-shots");
  static CatItem itens[40];
  static CatFileira fils[2];
  SDL_GLContext gl;
  char png[800], cache[700];
  int i, k, total = 0;

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-estiloanim-shot")) {
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

  ponteiro_iniciar();
  ponteiro_teste_janela(1920, 1080);
  ponteiro_teste_toque(1);
  ctx_fileira("pop_movie", "Em alta - Filme");
  ctx_abrir(6);
  quadros(40, NULL);
  confere("o menu abriu", ctx_aberto());
  quadros(40, NULL);
  { SDL_Event e; GfxRect r; float t0, prev = -1, t;
    int k, mono = 1, f[3] = { 3, 7, 11 }, fi = 0;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYUP; e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);   // solta o OK da abertura
    for (k = 0; k < 9; k++) {
      e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; ctx_evento(&e);
      e.type = SDL_KEYUP; ctx_evento(&e); quadros(4, NULL);
    }
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
    e.type = SDL_KEYUP; ctx_evento(&e);
    t0 = ctx_estilo_t();
    printf("  t(abrir)=%.3f\n", t0);
    confere("seletor aberto", ctx_estilo_rect(&r));
    confere("primeiro quadro nao esta em alfa cheio", t0 < 0.2f);
    for (k = 1; k <= 20; k++) {
      snprintf(png, sizeof png, "%s/estiloanim-f%02d.png", saida, k);
      quadros(1, (fi < 3 && k == f[fi]) ? png : NULL);
      if (fi < 3 && k == f[fi]) fi++;
      t = ctx_estilo_t();
      if (t + 1e-6f < prev) mono = 0;
      prev = t;
      if (k == 4 || k == 12 || k == 20) printf("  t(f%02d)=%.3f\n", k, t);
    }
    confere("progresso monotono", mono);
    confere("progresso chega perto de 1 em ~330 ms", ctx_estilo_t() > 0.95f);
    quadros(60, NULL);
    confere("progresso final = 1", ctx_estilo_t() > 0.999f);
    ctx_estilo_rect(&r);
    snprintf(png, sizeof png, "%s/estiloanim-final.png", saida);
    quadros(1, png);
    // Focus a sized row so both row and size targets exist before Back.
    const PonteiroAlvo *alvos;
    PonteiroAlvo row = {0};
    FilEstiloLinha linhas[FIL_TIPO_N];
    int nLinhas = fil_estilo_linhas("pop_movie", linhas, FIL_TIPO_N), sizedRow = -1;
    int nAlvos = ponteiro_teste_lista(&alvos), original = fil_tipo("pop_movie");
    for (k = 0; k < nLinhas; k++)
      if (linhas[k].n > 1) { sizedRow = k; break; }
    for (k = 0; k < nAlvos; k++)
      if (alvos[k].focar && alvos[k].ativar && alvos[k].a == sizedRow && alvos[k].b == 0)
        row = alvos[k];
    confere("picker row target exists while open", row.focar != NULL);
    if (row.focar) row.focar(row.a, row.b);
    quadros(30, NULL);
    int sizes = 0;
    nAlvos = ponteiro_teste_lista(&alvos);
    for (k = 0; k < nAlvos; k++)
      if (alvos[k].focar == row.focar && alvos[k].b > 0) sizes++;
    confere("size targets exist while open", sizes > 0);
    // VOLTAR: o seletor fecha animado (inverso da abertura), nao some de vez.
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e);
    e.type = SDL_KEYUP; ctx_evento(&e);
    long meio, fim;
    medir = r;
    quadros(1, NULL); t0 = ctx_estilo_t(); prev = 2; mono = 1; fi = 0;
    printf("  t(fechar)=%.3f\n", t0);
    confere("fechar nao zera o progresso de uma vez", t0 > 0.5f);
    meio = medido;
    int pickerTargets = 0;
    nAlvos = ponteiro_teste_lista(&alvos);
    for (k = 0; k < nAlvos; k++)
      if (alvos[k].focar == row.focar) pickerTargets++;
    confere("closing picker has no row, size or order targets", pickerTargets == 0);
    for (k = 1; k <= 20; k++) {
      snprintf(png, sizeof png, "%s/estiloanim-close-f%02d.png", saida, k);
      quadros(1, (fi < 3 && k == f[fi]) ? png : NULL);
      if (fi < 3 && k == f[fi]) fi++;
      t = ctx_estilo_t();
      if (t > prev + 1e-6f) mono = 0;
      prev = t;
    }
    confere("fechar: progresso monotono", mono);
    quadros(60, NULL);
    confere("fechar: progresso final = 0", ctx_estilo_t() < 0.001f);
    // O veu do menu nao pode apagar o seletor que some: no 1o quadro do
    // fechamento o retangulo dele ainda e bem mais claro que o menu em repouso.
    fim = medido; medir.w = 0.0f;
    printf("  brilho meio=%ld fim=%ld rect=%.0f,%.0f %.0fx%.0f\n", meio, fim, r.x, r.y, r.w, r.h);
    confere("fechar: seletor ainda visivel no inicio do fechamento", meio > fim * 4);
    // Repeat close for the click: the parent backdrop may dismiss its menu.
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
    e.type = SDL_KEYUP; ctx_evento(&e);
    quadros(30, NULL);
    confere("picker reopened for fading-row click", ctx_estilo_rect(&r));
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e);
    e.type = SDL_KEYUP; ctx_evento(&e);
    quadros(1, NULL);
    confere("click occurs while picker alpha exceeds 0.5", ctx_estilo_t() > 0.5f);
    e.type = SDL_MOUSEBUTTONDOWN;
    e.button.button = SDL_BUTTON_LEFT;
    e.button.x = (int)(row.x + 30);
    e.button.y = (int)(row.y + row.h / 2 + (1.0f - ctx_estilo_t()) * 12.0f);
    ponteiro_evento(&e, ctx_evento);
    e.type = SDL_MOUSEBUTTONUP;
    ponteiro_evento(&e, ctx_evento);
    confere("clicking fading row does not apply a style", fil_tipo("pop_movie") == original);
  }
  (void)gl;
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
