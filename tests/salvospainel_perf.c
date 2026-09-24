// MEDIDA DO PAINEL DE SALVOS (tecla AZUL) POR QUADRO, com a home por baixo.
//
// Existe por causa da C9 do dono (24/09/2026): com o painel aberto sobre uma
// lista de 121 titulos a TV caia de 60 para 52-54 fps e o [quadro] mostrava
// `upd` subindo de ~2 ms (home sozinha) para ~5 ms constante, mesmo parado.
// A captura de tela da TV custa ~74 ms na fase `aux` e polui a medida, entao a
// conta e feita aqui, no Mac, com um catalogo do tamanho do da TV:
//
//   16 fileiras x 125 = 2000 itens no catalogo, 117 titulos marcados naLista
//   pela "conta" (varios com uma SEGUNDA copia "ttX:1:2" com progresso, como a
//   fileira de Continuar assistindo guarda), mais 4 na lista local = 121.
//
// Mede separado: atualizar (home + painel), desenhar (CPU) e glFinish (o que a
// GPU ainda devia ao fim do quadro). O D-pad desce uma linha a cada 6 quadros
// (~10 teclas/s, o repeat de tecla presa), que e o gesto de quem navega.
//
// Com -DSP_CONTADOR (a arvore atual) tambem conta quantas vezes o painel
// reconstruiu a lista: parado, com o catalogo estavel, tem de ser ZERO. Sem a
// bandeira compila contra arvores antigas (v1.4.5) para a comparacao.
//
//   bash tests/salvospainel_perf.sh
#include "ajustes.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "salvos.h"
#include "salvospainel.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define NFIL   16
#define PORFIL 125
#define NCAT   (NFIL * PORFIL)

static double perFreq;
static double ms(Uint64 a, Uint64 b) { return (double)(b - a) * 1000.0 / perFreq; }

typedef struct { double upd, des, gpu, tot, fill; } Quadro;

static int cmpd(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}
static double pct(double *v, int n, double p) {
  qsort(v, (size_t)n, sizeof *v, cmpd);
  return v[(int)(p * (n - 1))];
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  spainel_evento(&e);
}

static void guardar(const char *nome);
static int comHome = 1, fundoParado = 1;
static void homeFundo(void *ctx) { (void)ctx; if (comHome) home_desenhar(SDL_GetTicks()); }

static void quadro(SDL_Window *w, Quadro *q) {
  Uint64 t0, t1, t2, t3;
  float dt = 1.0f / 60.0f;
  SDL_PumpEvents();
  tex_bombear(3);
  t0 = SDL_GetPerformanceCounter();
  if (comHome) home_atualizar(dt, SDL_GetTicks());
  spainel_atualizar(dt, SDL_GetTicks());
  t1 = SDL_GetPerformanceCounter();
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
#ifdef SP_CONTADOR
  // O MESMO caminho de app.c: o fundo passa por spainel_fundo.
  spainel_fundo(fundoParado, cat_revisao(), homeFundo, NULL);
#else
  homeFundo(NULL);
#endif
  spainel_desenhar(SDL_GetTicks());
  t2 = SDL_GetPerformanceCounter();
  glFinish();
  t3 = SDL_GetPerformanceCounter();
  if (!q && getenv("PERF_BMP_AGORA")) { guardar(getenv("PERF_BMP_AGORA")); unsetenv("PERF_BMP_AGORA"); }
  SDL_GL_SwapWindow(w);
  if (q) { q->upd = ms(t0, t1); q->des = ms(t1, t2); q->gpu = ms(t2, t3); q->tot = ms(t0, t3); q->fill = gfx_fill; }
}

// PERF_BMP=arquivo: guarda o quadro do painel parado, para comparar o fundo
// parado com o desenhado (PERF_SEM_FUNDO=1) pixel a pixel.
static void guardar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *sf = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && sf);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)sf->pixels + y * sf->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(sf, nome) == 0);
  SDL_FreeSurface(sf);
  free(pix);
  printf("captura: %s\n", nome);
}

static void povoar(void) {
  static CatItem itens[NCAT];
  static CatFileira fils[NFIL];
  int f, i, marcados = 0, comProg = 0;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (f = 0; f < NFIL; f++) {
    snprintf(fils[f].chave, sizeof fils[f].chave, "perf_movie_%02d", f);
    snprintf(fils[f].titulo, sizeof fils[f].titulo, "Fileira %d", f);
    snprintf(fils[f].tipo, sizeof fils[f].tipo, "movie");
    fils[f].ini = f * PORFIL;
    fils[f].n = PORFIL;
    for (i = 0; i < PORFIL; i++) {
      int k = f * PORFIL + i;
      CatItem *c = &itens[k];
      // A PRIMEIRA fileira e "Continuar assistindo": ids COMPOSTOS com
      // progresso, que casam por titulo com a copia simples de outra fileira.
      // As outras repetem um conjunto de 900 titulos, como fileiras reais de
      // addons se sobrepoem.
      int titulo = f == 0 ? i * 7 % 900 : (k * 13) % 900;
      if (f == 0)
        snprintf(c->imdb, sizeof c->imdb, "tt%07d:1:%d", 1000000 + titulo, i % 9 + 1);
      else
        snprintf(c->imdb, sizeof c->imdb, "tt%07d", 1000000 + titulo);
      snprintf(c->tipo, sizeof c->tipo, "%s", titulo % 3 ? "movie" : "series");
      snprintf(c->titulo, sizeof c->titulo, "Titulo de teste numero %d", titulo);
      snprintf(c->meta, sizeof c->meta, "%d", 1990 + titulo % 35);
      snprintf(c->genero, sizeof c->genero, "Filme · Drama");
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", titulo % 40);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (titulo + 7) % 40);
      c->nota = 50 + titulo % 45;
      if (f == 0 && i < 12) { c->progresso = 10 + i * 6; c->restanteMin = 20 + i; comProg++; }
      // 117 titulos da "conta": os 117 primeiros de 900, marcados em TODA
      // copia (a marca da conta cai em cat_indice_por_imdb, e a watchlist do
      // Trakt marca a copia da propria fileira).
      if (titulo < 117) { c->naLista = 1; marcados++; }
    }
  }
  cat_definir_tudo(itens, NCAT, fils, NFIL);
  // cat_definir_tudo zera naLista (e o contrato: a marca e reaplicada pelos
  // reconciliadores). Reaplica como a conta faria.
  for (i = 0; i < cat_n(); i++) {
    const CatItem *c = cat_item(i);
    int t = atoi(c->imdb + 2) - 1000000;
    if (t >= 0 && t < 117) cat_definir_na_lista(i, 1);
  }
  printf("catalogo: %d itens, %d copias marcadas, %d com progresso\n",
         cat_n(), marcados, comProg);
}

static void semearLocais(void) {
  int i;
  for (i = 0; i < 4; i++) {
    CatItem c;
    memset(&c, 0, sizeof c);
    // Dois locais que o catalogo tem (um deles com progresso na fileira 0) e
    // dois que ele nao tem.
    snprintf(c.imdb, sizeof c.imdb, "tt%07d", i < 2 ? 1000000 + i * 7 : 2000000 + i);
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "Salvo local %d", i);
    snprintf(c.poster, sizeof c.poster, "deploy/app/art/%02d.jpg", 40 + i);
    salvos_definir(&c, 1);
  }
}

static void relatar(const char *rotulo, Quadro *qs, int n) {
  double *v = malloc(sizeof(double) * (size_t)n);
  double su = 0, sd = 0, sg = 0, sf = 0;
  int i, itens = 0, pend = 0, quentes = 0;
  long bytes = 0, bq = 0;
  for (i = 0; i < n; i++) { su += qs[i].upd; sd += qs[i].des; sg += qs[i].gpu; sf += qs[i].fill; }
  printf("%-22s upd med=%.3f", rotulo, su / n);
  for (i = 0; i < n; i++) v[i] = qs[i].upd;
  printf(" p95=%.3f max=%.3f", pct(v, n, 0.95), v[n - 1]);
  for (i = 0; i < n; i++) v[i] = qs[i].des;
  printf(" | des med=%.3f p95=%.3f", sd / n, pct(v, n, 0.95));
  for (i = 0; i < n; i++) v[i] = qs[i].gpu;
  printf(" | gpu med=%.3f p95=%.3f ms | fill=%.2fx", sg / n, pct(v, n, 0.95), sf / n);
  tex_estatisticas(&itens, &pend, &bytes, &quentes, &bq);
  printf(" | tex %d %.1fMB quentes=%d %.1fMB\n", itens, bytes / 1048576.0, quentes, bq / 1048576.0);
  free(v);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  static Quadro qs[600];
  int i, n = 600;
  (void)argc; (void)argv;
  if (getenv("PERF_SEM_HOME")) comHome = 0;
  // PERF_SEM_FUNDO=1: sem o fundo parado, para medir o antes na mesma arvore.
  if (getenv("PERF_SEM_FUNDO")) fundoParado = 0;
  // ESCREVE salvos.txt: recusa rodar fora da pasta temporaria.
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("dados_dir() != NUVIO_DADOS; recusando\n"); return 2; }
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\n"); fclose(f);
    ajustes_dir(dir); }

  perFreq = (double)SDL_GetPerformanceFrequency();
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: painel de Salvos (medida)", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
#ifdef SP_CONTADOR
  // O mesmo FBO que main.c cria (o do fundo parado).
  gfx_snap_iniciar((int)NV_TELA_W, (int)NV_TELA_H);
#endif
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_icones_dir("deploy/app/art");
  if (comHome) assert(home_iniciar("deploy/app/art"));
  salvos_iniciar();
  povoar();
  semearLocais();
  salvos_aplicar_catalogo();

  // Home assentada antes de abrir: sem isto a medida pega a subida das artes.
  for (i = 0; i < 120; i++) quadro(w, NULL);
  if (comHome) {
    for (i = 0; i < n; i++) quadro(w, &qs[i]);
    relatar("home sozinha", qs, n);
  }

  spainel_abrir();
  for (i = 0; i < 90; i++) {
    if (i == 89 && getenv("PERF_BMP")) setenv("PERF_BMP_AGORA", getenv("PERF_BMP"), 1);
    quadro(w, NULL);
  }
#ifdef SP_CONTADOR
  { int r0 = spainel_n_reconstrucoes();
#endif
  for (i = 0; i < n; i++) quadro(w, &qs[i]);
  relatar("painel parado", qs, n);
#ifdef SP_CONTADOR
  printf("reconstrucoes com o painel parado: %d em %d quadros\n",
         spainel_n_reconstrucoes() - r0, n);
  r0 = spainel_n_reconstrucoes();
#endif
  for (i = 0; i < n; i++) {
    if (i % 6 == 0) tecla(i < n / 2 ? SDLK_DOWN : SDLK_UP);
    quadro(w, &qs[i]);
  }
  relatar("painel navegando", qs, n);
#ifdef SP_CONTADOR
  printf("reconstrucoes navegando: %d em %d quadros; fundo pintado %d vez(es) desde a abertura\n",
         spainel_n_reconstrucoes() - r0, n, spainel_n_fundos());
  }
#endif

  // O CUSTO DE UMA RECONSTRUCAO: fechar/abrir chama reconstruir() uma vez.
  { Uint64 a, b; int k, rep = 50;
    a = SDL_GetPerformanceCounter();
    for (k = 0; k < rep; k++) { spainel_fechar(); spainel_abrir(); }
    b = SDL_GetPerformanceCounter();
    printf("reconstruir (abrir): %.3f ms cada\n", ms(a, b) / rep); }

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
