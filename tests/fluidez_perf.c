// MEDIDA DE CUSTO DE DESENHO DA HOME E DO DETALHE, no Mac, por cenario.
//
// Existe pela queixa do testador da 1.6 ("the 1.6 polishes make the app
// slower", Samsung 2022) e pelos registros de campo: a 1.6 pinta mais telas
// cheias por quadro que a 1.5.4 (webOS 3,5 -> 4,9 telas; [gpu-modos] fill).
// Aqui a conta e feita SEM TV: preenchimento submetido por modo de desenho
// (gfx_fill_modo), desenhos, trocas de programa, quantos desenhos de tela
// cheia com mistura, e o tempo de CPU de desenhar. A GPU do Mac nao e a Mali,
// entao o glFinish sai so como referencia; o que vale comparar entre dois
// commits e o preenchimento e a contagem, que sao deterministicos.
//
//   bash tests/fluidez_perf.sh                 # todos os cenarios
//   NV_CENARIO=imersiva bash tests/fluidez_perf.sh
//   PERF_BMP=/tmp/x bash tests/fluidez_perf.sh # guarda /tmp/x-<cenario>.bmp
//
// Cenarios (home parada e navegando com a seta, e o detalhe aberto):
//   moderna   layout Moderna, destaque em tela cheia, tema Branco, sem vidro
//   imersiva  Moderna + "Dinamica imersiva" + vidro (a C9 do dono, 30/09)
//   padrao    layout Padrao
//   dinamica  layout Dinamica
//   dono      Dinamica + "Dinamica imersiva" + vidro: a TCL Smart TV Pro do dono
//             (Android 14, Mali-G52; [gpu-modos] layout=2 cor-viva=4 vidro=1)
//   c9        Dinamica + "Dinamica imersiva" SEM vidro: a LG C9 do dono
//             ([gpu-modos] layout=2 cor-viva=4 vidro=0, 04/10)
//   frost     o c9 com Ajustes > Fundo = Frost (paginas do titulo e Ajustes)
//   borrada   o c9 com Ajustes > Fundo = Arte borrada
//
// Cada fase sai com media, p95 e o PIOR quadro (des+upd) com o que ele fez:
// texto rasterizado, artes enviadas a GPU, desenhos. Depois da home de cada
// cenario, a lista dos Ajustes e rolada para baixo (uma seta a cada 8 quadros)
// e para cima. NV_QUADROS=1 imprime todo quadro acima de 2 ms.
#include "agendaui.h"
#include "ajustes.h"
#include "biblioteca.h"
#include "menu.h"
#include "salvospainel.h"
#include "catalogo.h"
#include "corviva.h"
#include "dados.h"
#include "detail.h"
#include "extras.h"   // extras_shot_relacionados (NV_SHOT_HOOKS): fileiras do detalhe
#include "fpscena.h"
#include "fundo.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "perfis.h"    // perfis_carregar_ativo (cena perfil)
#include "perfilsel.h"  // a tela de escolha, medida (#2 do handoff 1.8)
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NFIL   8
#define PORFIL 24
#define NCAT   (NFIL * PORFIL)

static double perFreq;
static double ms(Uint64 a, Uint64 b) { return (double)(b - a) * 1000.0 / perFreq; }
static const char *dirDados;

typedef struct {
  double upd, des, gpu, fill, fillVis, fillGpu, fillGpuMist;
  int rects, progs, binds, cheios, cheiosMist, txtRast, upl, vidCap, vidPas;
  double txtMs;
  double modo[GFX_NMODOS];
} Quadro;
static int naAjustes;
// OUTRAS TELAS (04/10): Biblioteca e Agenda no lugar da home; painel de Salvos
// e menu lateral POR CIMA da home, como em app.c.
typedef struct {
  void (*evento)(const SDL_Event *);
  void (*atualizar)(float, Uint32);
  void (*desenhar)(Uint32);
  int sobreHome;
} Tela;
static const Tela *telaAtual;

// Desenhos de tela cheia COM mistura: e a conta que a Mali paga a mais (uma
// leitura da tela por pixel). Medido pelo gfx_rect via gancho de contagem.
// -DNV_PERF_BASE: arvore antiga, sem o contador nem a luz pendente.
#ifdef NV_PERF_BASE
static int gfx_n_cheio_mistura = -1;
#define gfx_ambiente_descarregar() ((void)0)
#else
extern int gfx_n_cheio_mistura;
#endif

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (telaAtual) { telaAtual->evento(&e); e.type = SDL_KEYUP; telaAtual->evento(&e); return; }
  if (naAjustes) { ajustes_evento(&e); return; }
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
  e.type = SDL_KEYUP;
  if (detail_aberto()) detail_evento(&e); else home_evento(&e);
}

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

// O MESMO caminho de main.c, fase a fase.
static void quadro(SDL_Window *w, Quadro *q, Uint32 agora) {
  Uint64 t0, t1, t2, t3;
  float dt = 1.0f / 60.0f;
  int k;
  SDL_PumpEvents();
  tex_upl_n = 0;
  tex_bombear(3);
  txt_rasterizadas = 0; txt_ms = 0.0;
  t0 = SDL_GetPerformanceCounter();
  if (telaAtual) {
    if (telaAtual->sobreHome) home_atualizar(dt, agora);
    telaAtual->atualizar(dt, agora);
  } else if (naAjustes) ajustes_atualizar(dt, agora);
  else { home_atualizar(dt, agora); detail_atualizar(dt, agora); }
  corviva_quadro(dt, ajustes_cor_viva(), ajustes_cor_logo(), ajustes_animacoes_reduzidas());
  t1 = SDL_GetPerformanceCounter();
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  gfx_ambiente_preparar();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_ambiente(1.0f);
  txt_novo_quadro();
  if (telaAtual) {
    if (telaAtual->sobreHome) home_desenhar(agora);
    telaAtual->desenhar(agora);
  } else if (naAjustes) ajustes_desenhar(agora);
  else { if (!detail_cobre_tela()) home_desenhar(agora); detail_desenhar(agora); }
  gfx_ambiente_descarregar();
  t2 = SDL_GetPerformanceCounter();
  glFinish();
  t3 = SDL_GetPerformanceCounter();
  if (q) {
    q->upd = ms(t0, t1); q->des = ms(t1, t2); q->gpu = ms(t2, t3);
    q->fill = gfx_fill; q->fillVis = gfx_fill_vis;
    q->fillGpu = gfx_fill_gpu; q->fillGpuMist = gfx_fill_gpu_mist;
    q->rects = gfx_n_rect; q->progs = gfx_n_prog; q->binds = gfx_n_bind;
    q->cheios = gfx_n_cheio; q->cheiosMist = gfx_n_cheio_mistura;
    q->txtRast = txt_rasterizadas; q->txtMs = txt_ms; q->upl = tex_upl_n;
    q->vidCap = gfx_vidro_capturas; q->vidPas = gfx_vidro_passadas;
    for (k = 0; k < GFX_NMODOS; k++) q->modo[k] = gfx_fill_modo[k];
  }
  SDL_GL_SwapWindow(w);
}

static void povoar(void) {
  static CatItem itens[NCAT];
  static CatFileira fils[NFIL];
  int f, i;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (f = 0; f < NFIL; f++) {
    snprintf(fils[f].chave, sizeof fils[f].chave, "perf_%02d", f);
    snprintf(fils[f].titulo, sizeof fils[f].titulo, "Fileira de teste %d - Filme", f);
    snprintf(fils[f].tipo, sizeof fils[f].tipo, "movie");
    fils[f].ini = f * PORFIL;
    fils[f].n = PORFIL;
    for (i = 0; i < PORFIL; i++) {
      int k = f * PORFIL + i, t = (k * 13) % 900;
      CatItem *c = &itens[k];
      snprintf(c->imdb, sizeof c->imdb, "tt%07d", 1000000 + t);
      snprintf(c->tipo, sizeof c->tipo, "%s", t % 3 ? "movie" : "series");
      snprintf(c->titulo, sizeof c->titulo, "Titulo de teste numero %d", t);
      snprintf(c->meta, sizeof c->meta, "%d", 1990 + t % 35);
      snprintf(c->genero, sizeof c->genero, "Filme · Drama");
      snprintf(c->sinopse, sizeof c->sinopse,
               "Uma sinopse de teste longa o bastante para ocupar tres linhas do "
               "destaque e medir o texto que a home rasteriza e guarda por titulo %d.", t);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", t % 40);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (t + 7) % 40);
      snprintf(c->classificacao, sizeof c->classificacao, "%s", t % 2 ? "14" : "");
      c->nota = 50 + t % 45;
      if (f == 0 && i < 6) { c->progresso = 10 + i * 6; c->restanteMin = 20 + i; }
      // O DETALHE DA TV NAO E VAZIO (#2 do handoff de desempenho 1.8,
      // 05/10): na C9 a pagina do titulo mede 7,4 telas/quadro com fileiras
      // de arte (relacionados, elenco), e o fixture media 3,0 porque nenhum
      // desses dados existia aqui. O ELENCO vem do CatItem (catalogo.h) e
      // as TEMPORADAS tambem; a foto e local como o poster. Sem isto a
      // pagina do fixture nao representa a da TV e todo numero de detalhe
      // sai otimista.
      if (i < 6) {
        c->nElenco = 6;
        for (int e = 0; e < 6; e++)
          snprintf(c->elenco[e].foto, sizeof c->elenco[e].foto,
                   "deploy/app/art/elenco/%02d_%d.jpg", i, e);
        snprintf(c->elenco[0].nome, sizeof c->elenco[0].nome, "Ator de Teste Um");
        snprintf(c->elenco[1].nome, sizeof c->elenco[1].nome, "Atora de Teste Dois");
        snprintf(c->elenco[2].nome, sizeof c->elenco[2].nome, "Elenco Tres");
        snprintf(c->elenco[3].nome, sizeof c->elenco[3].nome, "Elenco Quatro");
        snprintf(c->elenco[4].nome, sizeof c->elenco[4].nome, "Elenco Cinco");
        snprintf(c->elenco[5].nome, sizeof c->elenco[5].nome, "Elenco Seis");
      }
      if (!(t % 3)) {   // series: a pagina ganha as fileiras de temporada
        c->nTemporadas = 3;
        for (int tp = 0; tp < 3; tp++) c->temporadas[tp] = tp + 1;
      }
    }
  }
  cat_definir_tudo(itens, NCAT, fils, NFIL);
  // RELACIONADOS com arte: o mesmo hook das capturas (NV_SHOT_HOOKS). Sem ele
  // a secao "Recomendacoes" nao existe no fixture e a TV desenha 8 cards.
  { static const char *rt[8] = { "Relacionado Um", "Relacionado Dois",
                                 "Relacionado Tres", "Relacionado Quatro",
                                 "Relacionado Cinco", "Relacionado Seis",
                                 "Relacionado Sete", "Relacionado Oito" };
    static const char *ra[8] = { "2020", "2019", "2021", "2018",
                                 "2022", "2017", "2023", "2016" };
    static const char *rp[8] = { "deploy/app/art/12.jpg", "deploy/app/art/10.jpg",
                                 "deploy/app/art/16.jpg", "deploy/app/art/14.jpg",
                                 "deploy/app/art/32.jpg", "deploy/app/art/03.jpg",
                                 "deploy/app/art/36.jpg", "deploy/app/art/02.jpg" };
    extras_shot_relacionados(rt, ra, rp, 8); }
}

static void ajusta(const char *cenario) {
  char cam[700];
  FILE *a;
  int layout = 0, vidro = 0, tema = 0, cheio = 1, fundo = 0;
  if (!strcmp(cenario, "imersiva")) { vidro = 1; tema = 15; }
  else if (!strcmp(cenario, "dono")) { layout = 2; vidro = 1; tema = 15; }
  else if (!strcmp(cenario, "c9")) { layout = 2; vidro = 0; tema = 15; }
  else if (!strcmp(cenario, "frost")) { layout = 2; vidro = 0; tema = 15; fundo = 2; }
  else if (!strcmp(cenario, "borrada")) { layout = 2; vidro = 0; tema = 15; fundo = 1; }
  else if (!strcmp(cenario, "padrao")) layout = 1;
  else if (!strcmp(cenario, "dinamica")) layout = 2;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  // No arquivo, 0 = LIGADO nos ajustes de liga/desliga (V_LIGA).
  fprintf(a, "idioma 0\ntrailerHero 1\nhomeLayoutLocal %d\nvidroLocal %d\n"
             "modernLandscapePostersEnabled 1\nmodernHeroFullScreenBackdropEnabled %d\n"
             "selected_theme %d\ncardDepthEnabled 0\nposterLabelsEnabled 0\nfundoLocal %d\n",
          layout, vidro ? 0 : 1, cheio ? 0 : 1, tema, fundo);
  if (getenv("NV_AJ")) fprintf(a, "%s\n", getenv("NV_AJ"));
  fclose(a);
  ajustes_dir(dirDados);
}

static int cmpd(const void *a, const void *b) {
  double x = *(const double *)a, y = *(const double *)b;
  return x < y ? -1 : x > y;
}
static double p95(double *v, int n) { qsort(v, (size_t)n, sizeof *v, cmpd); return v[(int)(0.95 * (n - 1))]; }

// FPS GATE (fpscena.h): every reported phase also prints its percentiles and a
// `FPSBASE <cena> <p95> <upl>` line. Save the output and compare it offline
// with tests/fps_gate.sh. These CPU samples exclude GPU/swap waits; never
// compare this baseline with the TV's full frame intervals.
static void fps_gate_fase(const char *cen, const char *rotulo, const Quadro *qs, int n) {
  static FpsAmostra am;
  char nome[96], l[400];
  int i;
  snprintf(nome, sizeof nome, "cpu/%s/%s", cen, rotulo);
  fps_zerar(&am);
  for (i = 0; i < n; i++) fps_add(&am, qs[i].des + qs[i].upd, qs[i].upl);
  fps_linha(&am, nome, l, sizeof l);
  printf("%s\n", l);
  printf("FPSBASE %s %.3f %ld\n", nome, fps_percentil(&am, 95), am.upl);
}

static void relatar(const char *cen, const char *rotulo, Quadro *qs, int n) {
  double *v = malloc(sizeof(double) * (size_t)n);
  double su = 0, sd = 0, sg = 0, sf = 0, sfv = 0, sfg = 0, sfm = 0, sm[GFX_NMODOS];
  long sr = 0, sp = 0, sb = 0, sc = 0, scm = 0, st = 0;
  int i, k;
  memset(sm, 0, sizeof sm);
  for (i = 0; i < n; i++) {
    su += qs[i].upd; sd += qs[i].des; sg += qs[i].gpu; sf += qs[i].fill; sfv += qs[i].fillVis;
    sfg += qs[i].fillGpu; sfm += qs[i].fillGpuMist;
    sr += qs[i].rects; sp += qs[i].progs; sb += qs[i].binds; sc += qs[i].cheios; scm += qs[i].cheiosMist;
    st += qs[i].txtRast;
    for (k = 0; k < GFX_NMODOS; k++) sm[k] += qs[i].modo[k];
  }
  fps_gate_fase(cen, rotulo, qs, n);
  { int pi = 0, su_upl = 0; double pm = -1;
    for (i = 0; i < n; i++) su_upl += qs[i].upl;
    for (i = 0; i < n; i++) if (qs[i].des + qs[i].upd > pm) { pm = qs[i].des + qs[i].upd; pi = i; }
    for (i = 0; i < n; i++) v[i] = qs[i].des + qs[i].upd;
    qsort(v, (size_t)n, sizeof *v, cmpd);
    printf("[%s] %-10s cpu/quadro: mediana=%.2f p95=%.2f pior=%.2f ms (quadro %d: upd=%.2f des=%.2f"
           " txt=%d/%.2fms upl=%d rects=%d) | txt total=%ld upl total=%d\n",
           cen, rotulo, v[n / 2], v[(int)(0.95 * (n - 1))], pm, pi, qs[pi].upd, qs[pi].des,
           qs[pi].txtRast, qs[pi].txtMs, qs[pi].upl, qs[pi].rects, st, su_upl);
    if (getenv("NV_QUADROS"))
      for (i = 0; i < n; i++) if (qs[i].des + qs[i].upd > 2.0)
        printf("[%s] %-10s  q%03d upd=%.2f des=%.2f txt=%d/%.2fms upl=%d\n", cen, rotulo, i,
               qs[i].upd, qs[i].des, qs[i].txtRast, qs[i].txtMs, qs[i].upl); }
  for (i = 0; i < n; i++) v[i] = qs[i].des;
  printf("[%s] %-10s upd=%.2f des=%.2f(p95 %.2f) gpu=%.2f ms | fill=%.2fx vis=%.2fx cheios=%.1f mist=%.1f"
         " | rects=%ld progs=%ld binds=%ld txt=%ld\n",
         cen, rotulo, su / n, sd / n, p95(v, n), sg / n, sf / n, sfv / n,
         (double)sc / n, (double)scm / n, sr / n, sp / n, sb / n, st);
  // O que a GPU pinta de fato (gfx.h, gfx_fill_gpu) e a parte com mistura.
  printf("[%s] %-10s gpu-fill=%.2fx mist=%.2fx\n", cen, rotulo, sfg / n, sfm / n);
  { long vc = 0, vp = 0; int qc = 0;
    for (i = 0; i < n; i++) { vc += qs[i].vidCap; vp += qs[i].vidPas; qc += qs[i].vidCap > 0; }
    printf("[%s] %-10s vidro: capturas=%ld passadas=%ld quadros-com-captura=%d/%d\n", cen, rotulo, vc, vp, qc, n); }
  printf("[%s] %-10s fill por modo:", cen, rotulo);
  for (k = 0; k < GFX_NMODOS; k++) if (sm[k] / n >= 0.005) printf(" %d=%.2f", k, sm[k] / n);
  printf("\n");
  free(v);
}

static void cenario(SDL_Window *w, const char *cen) {
  static Quadro qs[300];
  int i, n = 300;
  Uint32 t = 1000;
  ajusta(cen);
  home_ir_topo();
  // Assenta: artes sobem, molas param.
  for (i = 0; i < 240; i++) { quadro(w, NULL, t); t += 16; }
  // O DESTAQUE NO TOPO, parado: no layout Dinamica e o fundo + a arte de tela
  // cheia (GFX_VITRINE / GFX_VITRINE_DIN), a tela que a TCL mede ao abrir.
  for (i = 0; i < 120; i++) { quadro(w, &qs[i], t); t += 16; }
  relatar(cen, "topo", qs, 120);
  if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
  if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-topo.bmp", getenv("PERF_BMP"), cen); guardar(b); }
  // Primeira fileira em foco (o destaque sai do foco), depois parado.
  tecla(SDLK_DOWN);
  for (i = 0; i < 120; i++) { quadro(w, NULL, t); t += 16; }
  for (i = 0; i < n; i++) { quadro(w, &qs[i], t); t += 16; }
  relatar(cen, "parada", qs, n);
  if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
  if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-parada.bmp", getenv("PERF_BMP"), cen); guardar(b); }
  // CROSSFADE DO DESTAQUE, no meio: um passo a direita (a arte nova sobe e o
  // esvanecimento corre ate o fim), volta a esquerda (arte ja em cache, o
  // esvanecimento comeca na hora) e captura no 4o e no 8o quadro da volta.
  // E a mesma sequencia nas duas arvores; a captura compara a composicao em
  // camadas (gfx_hero_camadas) com as duas passadas misturadas.
  if (getenv("PERF_BMP")) {
    char b[800];
    tecla(SDLK_RIGHT);
    for (i = 0; i < 90; i++) { quadro(w, NULL, t); t += 16; }
    tecla(SDLK_LEFT);
    for (i = 0; i < 4; i++) { quadro(w, NULL, t); t += 16; }
    snprintf(b, sizeof b, "%s-%s-xfade4.bmp", getenv("PERF_BMP"), cen); guardar(b);
    for (i = 0; i < 4; i++) { quadro(w, NULL, t); t += 16; }
    snprintf(b, sizeof b, "%s-%s-xfade8.bmp", getenv("PERF_BMP"), cen); guardar(b);
    for (i = 0; i < 90; i++) { quadro(w, NULL, t); t += 16; }
  }
  for (i = 0; i < n; i++) {
    if (i % 12 == 0) tecla(i < n / 2 ? SDLK_RIGHT : SDLK_LEFT);
    quadro(w, &qs[i], t); t += 16;
  }
  relatar(cen, "navegando", qs, n);
  // Detalhe por cima.
  { HomeItem it;
    if (home_item_focado(&it)) {
      detail_abrir(&it);
      for (i = 0; i < 180; i++) { quadro(w, NULL, t); t += 16; }
      for (i = 0; i < n; i++) { quadro(w, &qs[i], t); t += 16; }
      relatar(cen, "detalhe", qs, n);
      if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
      if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-detalhe.bmp", getenv("PERF_BMP"), cen); guardar(b); }
      // A PAGINA do titulo (seta para baixo: o cartao vira tela cheia), onde
      // entra Ajustes > Fundo (fundo.c).
      tecla(SDLK_DOWN);
      for (i = 0; i < 180; i++) { quadro(w, NULL, t); t += 16; }
      for (i = 0; i < n; i++) { quadro(w, &qs[i], t); t += 16; }
      relatar(cen, "pagina", qs, n);
      if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
      if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-pagina.bmp", getenv("PERF_BMP"), cen); guardar(b); }
      tecla(SDLK_AC_BACK);
      for (i = 0; i < 60; i++) { quadro(w, NULL, t); t += 16; }
      tecla(SDLK_AC_BACK);
      for (i = 0; i < 120; i++) { quadro(w, NULL, t); t += 16; }
    } else printf("[%s] sem item focado: detalhe nao medido\n", cen);
  }
  // AJUSTES: a lista rolada para baixo e de volta, uma seta a cada 8 quadros.
  ajustes_iniciar();
  naAjustes = 1;
  for (i = 0; i < 120; i++) { quadro(w, NULL, t); t += 16; }
  for (i = 0; i < n; i++) {
    if (i % 8 == 0) tecla(i < n / 2 ? SDLK_DOWN : SDLK_UP);
    quadro(w, &qs[i], t); t += 16;
  }
  relatar(cen, "ajustes", qs, n);
  if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
  if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-ajustes.bmp", getenv("PERF_BMP"), cen); guardar(b); }
  // CATEGORIAS: dentro de cada uma (lista + inspetor com a arte/previa), rolando.
  { static const int ordem[3] = { 0, 2, 4 };
    int c, k, pos = 0;
    for (c = 0; c < 3; c++) {
      char nome[32];
      while (pos > ordem[c]) { tecla(SDLK_UP); pos--; quadro(w, NULL, t); t += 16; }
      while (pos < ordem[c]) { tecla(SDLK_DOWN); pos++; quadro(w, NULL, t); t += 16; }
      for (i = 0; i < 40; i++) { quadro(w, NULL, t); t += 16; }
      tecla(SDLK_RETURN);
      for (i = 0; i < 120; i++) { quadro(w, NULL, t); t += 16; }
      for (i = 0; i < n; i++) {
        if (i % 8 == 0) tecla(i < n / 2 ? SDLK_DOWN : SDLK_UP);
        quadro(w, &qs[i], t); t += 16;
      }
      snprintf(nome, sizeof nome, "ajustes-cat%d", ordem[c]);
      relatar(cen, nome, qs, n);
      if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
      if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-%s.bmp", getenv("PERF_BMP"), cen, nome); guardar(b); }
      for (k = 0; k < 3; k++) { tecla(SDLK_AC_BACK); for (i = 0; i < 20; i++) { quadro(w, NULL, t); t += 16; } }
    }
  }
  ajustes_encerrar();
  naAjustes = 0;
  for (i = 0; i < 60; i++) { quadro(w, NULL, t); t += 16; }
  // ESCOLHA DE PERFIL (#2 do handoff de desempenho 1.8, 05/10): a TV do dono
  // mede esta tela a 7,4 telas/quadro (log [gpu-modos] tela=escolha-perfil:
  // sombra 2,46 + cor 2,05 + card 0,93 + luz ambiente 1,00, cor-viva=4) e o
  // harness nem a desenhava — era a maior divergencia fixture/TV do detalhe.
  // O mural usa os posters do CATALOGO (muralRecriar, cat_item) e os perfis
  // vem de perfis.txt no NUVIO_DADOS: 4 perfis com fundo/avatar locais, como
  // na conta de verdade.
  { FILE *f;
    char cam[700];
    snprintf(cam, sizeof cam, "%s/%s", dirDados, "perfis.txt");
    f = fopen(cam, "w");
    assert(f);
    fprintf(f,
      "1\t0\t1\t0\t#8A5CF6\tSala\tdeploy/app/art/00.jpg\tdeploy/app/art/03.jpg\n"
      "2\t0\t0\t0\t#F59E0B\tInfantil\tdeploy/app/art/01.jpg\tdeploy/app/art/07.jpg\n"
      "3\t0\t0\t0\t#10B981\tDocumentarios\tdeploy/app/art/02.jpg\tdeploy/app/art/11.jpg\n"
      "4\t0\t0\t0\t#EF4444\tPapo\tdeploy/app/art/04.jpg\tdeploy/app/art/15.jpg\n");
    fclose(f);
    perfis_carregar_ativo();
    perfilsel_iniciar();
    { static const Tela pf = { perfilsel_evento, perfilsel_atualizar, perfilsel_desenhar, 0 };
      telaAtual = &pf;
      for (i = 0; i < 180; i++) { quadro(w, NULL, t); t += 16; }   // entrada + burst assenta
      for (i = 0; i < n; i++) {
        if (i % 12 == 0) tecla(i < n / 2 ? SDLK_RIGHT : SDLK_LEFT);
        quadro(w, &qs[i], t); t += 16;
      }
      relatar(cen, "perfil", qs, n);
      if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
      if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-perfil.bmp", getenv("PERF_BMP"), cen); guardar(b); }
      telaAtual = NULL;
    }
    for (i = 0; i < 60; i++) { quadro(w, NULL, t); t += 16; }
  }
  // BIBLIOTECA, AGENDA, SALVOS e MENU: assenta, mede parado e com a seta.
  { static const Tela bib = { biblioteca_evento, biblioteca_atualizar, biblioteca_desenhar, 0 };
    static const Tela age = { agendaui_evento, agendaui_atualizar, agendaui_desenhar, 0 };
    static const Tela sal = { spainel_evento, spainel_atualizar, spainel_desenhar, 1 };
    static const Tela men = { menu_evento, menu_atualizar, menu_desenhar, 1 };
    const Tela *ts[4] = { &bib, &age, &sal, &men };
    static const char *const nomes[4] = { "biblioteca", "agenda", "salvos", "menu" };
    int k;
    for (k = 0; k < 4; k++) {
      if (k == 0) biblioteca_iniciar();
      else if (k == 1) agendaui_iniciar();
      else if (k == 2) spainel_abrir();
      else menu_abrir();
      telaAtual = ts[k];
      for (i = 0; i < 150; i++) { quadro(w, NULL, t); t += 16; }
      for (i = 0; i < n; i++) {
        if (i % 12 == 0) tecla(i < n / 2 ? SDLK_DOWN : SDLK_UP);
        quadro(w, &qs[i], t); t += 16;
      }
      relatar(cen, nomes[k], qs, n);
      if (getenv("NV_RASTRO")) { gfx_rastro_grandes = 1; quadro(w, NULL, t); t += 16; gfx_rastro_grandes = 0; }
      if (getenv("PERF_BMP")) { char b[800]; snprintf(b, sizeof b, "%s-%s-%s.bmp", getenv("PERF_BMP"), cen, nomes[k]); guardar(b); }
      if (k == 0) biblioteca_encerrar();
      else if (k == 2) spainel_fechar();
      else if (k == 3) menu_fechar();
      telaAtual = NULL;
      for (i = 0; i < 60; i++) { quadro(w, NULL, t); t += 16; }
    }
  }
}

int main(int argc, char **argv) {
  static const char *const todos[] = { "moderna", "imersiva", "padrao", "dinamica", "dono", "c9", "frost",
                                       "borrada" };
  const char *so = getenv("NV_CENARIO");
  SDL_Window *w;
  SDL_GLContext gl;
  int i;
  (void)argc; (void)argv;
  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-fluidez-perf")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  perFreq = (double)SDL_GetPerformanceFrequency();
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: fluidez (medida)", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar((int)NV_TELA_W / 2, (int)NV_TELA_H / 2);
  gfx_borrao_iniciar(480, 270);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_icones_dir("deploy/app/art");
  ajusta(so ? so : "moderna");
  assert(home_iniciar("deploy/app/art"));
  povoar();
  for (i = 0; i < 8; i++)
    if (!so || !strcmp(so, todos[i])) cenario(w, todos[i]);
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
