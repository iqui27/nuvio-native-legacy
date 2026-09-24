// Painel de Salvos (tecla AZUL): o que ele faz POR QUADRO com a lista parada.
//
// A C9 do dono (24/09/2026) navegava o painel a 52-54 fps contra 60 na home.
// Tres contratos sairam dali, e este teste cobra os tres:
//
//   1. RECONSTRUIR SO QUANDO MUDA. A lista (uniao lista local + catalogo) e
//      remontada quando a revisao do catalogo ou da lista local sobe — nunca
//      por quadro. Com 2000 itens no catalogo, parado ou navegando, zero.
//      Remarcar o que ja estava marcado (os reconciliadores fazem isso) nao
//      conta como mudanca; marcar um titulo novo, sim.
//   2. O FUNDO PARADO. Com o painel inteiro na tela, a home e pintada UMA vez
//      num FBO e os quadros seguintes so copiam — e a copia tem de ser a
//      mesma imagem que o desenho direto daria.
//   3. A copia e refeita quando o catalogo troca (fileiras novas por baixo) e
//      cai quando o painel fecha.
//
//   bash tests/salvospainel.sh
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

static int falhas;
static void confere(const char *o_que, int obtido, int esperado) {
  int ok = obtido == esperado;
  printf("  %-60s %s (obtido %d, esperado %d)\n", o_que, ok ? "ok   " : "FALHOU",
         obtido, esperado);
  if (!ok) falhas++;
}

#define NFIL   16
#define PORFIL 125
#define NCAT   (NFIL * PORFIL)

static int podeParar = 1;
static unsigned char *mascara;
static void homeFundo(void *ctx) { (void)ctx; home_desenhar(SDL_GetTicks()); }

static void quadro(SDL_Window *w) {
  SDL_PumpEvents();
  tex_bombear(3);
  home_atualizar(1.0f / 60.0f, SDL_GetTicks());
  spainel_atualizar(1.0f / 60.0f, SDL_GetTicks());
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
  spainel_fundo(podeParar, cat_revisao(), homeFundo, NULL);
  spainel_desenhar(SDL_GetTicks());
  (void)w;
}
static void quadros(SDL_Window *w, int n) {
  int i;
  for (i = 0; i < n; i++) { quadro(w); SDL_GL_SwapWindow(w); }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  spainel_evento(&e);
}

static CatItem itens[NCAT];
static CatFileira fils[NFIL];
static void montar(int deslocamento) {
  int f, i;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (f = 0; f < NFIL; f++) {
    snprintf(fils[f].chave, sizeof fils[f].chave, "teste_movie_%02d", f);
    snprintf(fils[f].titulo, sizeof fils[f].titulo, "Fileira %d", f);
    snprintf(fils[f].tipo, sizeof fils[f].tipo, "movie");
    fils[f].ini = f * PORFIL;
    fils[f].n = PORFIL;
    for (i = 0; i < PORFIL; i++) {
      int k = f * PORFIL + i, t = (k * 13 + deslocamento) % 900;
      CatItem *c = &itens[k];
      if (f == 0) snprintf(c->imdb, sizeof c->imdb, "tt%07d:1:%d", 1000000 + t, i % 9 + 1);
      else        snprintf(c->imdb, sizeof c->imdb, "tt%07d", 1000000 + t);
      snprintf(c->tipo, sizeof c->tipo, "movie");
      snprintf(c->titulo, sizeof c->titulo, "Titulo %d", t);
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", t % 40);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (t + 7) % 40);
      if (f == 0 && i < 12) { c->progresso = 20 + i; c->restanteMin = 30; }
    }
  }
  cat_definir_tudo(itens, NCAT, fils, NFIL);
  for (i = 0; i < cat_n(); i++) {
    int t = atoi(cat_item(i)->imdb + 2) - 1000000;
    if (t >= 0 && t < 117) cat_definir_na_lista(i, 1);
  }
}

static unsigned char *ler(void) {
  unsigned char *p = malloc(1920 * 1080 * 4);
  assert(p);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, p);
  return p;
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  GLuint fbo, fboTex;
  int r0, f0, i;
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("dados_dir() != NUVIO_DADOS; recusando\n"); return 2; }
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: painel de Salvos", 0, 0, 64, 64,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  // Janela escondida: o quadro vai para um FBO do tamanho da TV, que faz o
  // papel da tela. gfx_snap_terminar devolve o alvo que estava ligado (este),
  // e nao o 0 — e o que permite medir a copia por glReadPixels.
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_icones_dir("deploy/app/art");
  confere("o FBO do fundo parado existe", gfx_snap_iniciar(1920, 1080), 1);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  assert(home_iniciar("deploy/app/art"));
  salvos_iniciar();
  montar(0);
  printf("catalogo: %d itens\n", cat_n());
  quadros(w, 60);

  printf("\nreconstruir so quando muda:\n");
  spainel_abrir();
  r0 = spainel_n_reconstrucoes();
  quadros(w, 300);
  confere("300 quadros parado: nenhuma reconstrucao", spainel_n_reconstrucoes() - r0, 0);
  for (i = 0; i < 300; i++) { if (i % 6 == 0) tecla(i < 150 ? SDLK_DOWN : SDLK_UP); quadro(w); SDL_GL_SwapWindow(w); }
  confere("300 quadros navegando: nenhuma", spainel_n_reconstrucoes() - r0, 0);
  // O reconciliador remarca o que ja estava marcado: nao e mudanca.
  for (i = 0; i < cat_n(); i++)
    if (cat_item(i)->naLista) { cat_definir_na_lista(i, 1); }
  quadros(w, 10);
  confere("remarcar o que ja estava marcado: nenhuma", spainel_n_reconstrucoes() - r0, 0);
  // Um titulo novo marcado pela conta com o painel aberto.
  for (i = 0; i < cat_n(); i++)
    if (!cat_item(i)->naLista) { cat_definir_na_lista(i, 1); break; }
  quadros(w, 10);
  confere("marcar um titulo novo: uma", spainel_n_reconstrucoes() - r0, 1);
  { CatItem c; memset(&c, 0, sizeof c);
    snprintf(c.imdb, sizeof c.imdb, "tt7777777");
    snprintf(c.tipo, sizeof c.tipo, "movie");
    snprintf(c.titulo, sizeof c.titulo, "Salvo agora");
    salvos_definir(&c, 1); }
  quadros(w, 10);
  confere("salvar na lista local: mais uma", spainel_n_reconstrucoes() - r0, 2);

  printf("\no fundo parado:\n");
  // As tres repinturas da abertura (0,4 s, 1,5 s e 4 s) ja passaram.
  SDL_Delay(4200);
  quadros(w, 3);
  f0 = spainel_n_fundos();
  quadros(w, 300);
  confere("depois da abertura, 300 quadros sem pintar o fundo", spainel_n_fundos() - f0, 0);
  // A COPIA E A MESMA IMAGEM. O quadro que pinta a copia e o quadro seguinte
  // com o fundo desenhado direto (podeParar=0) — mesma home, mesmo painel.
  { unsigned char *a, *b;
    int k, pior = 0, difs = 0, quietos = 0, voltas = 0;
    // A arte da home sobe em fio (tex_bombear): uma textura que chega entre
    // os dois quadros mudaria a imagem sem ter nada a ver com a copia. Espera
    // a fila esvaziar e ficar vazia por 30 quadros.
    podeParar = 0;
    while (quietos < 30 && voltas++ < 3000) {
      int it = 0, pend = 0, q = 0; long by = 0, bq = 0;
      tex_upl_n = 0;
      quadro(w); SDL_GL_SwapWindow(w);
      tex_estatisticas(&it, &pend, &by, &q, &bq);
      quietos = (pend == 0 && tex_upl_n == 0) ? quietos + 1 : 0;
      SDL_Delay(2);
    }
    // O QUE A HOME ANIMA SOZINHA fica de fora da comparacao: o esqueleto dos
    // cartazes da fileira cortada no pe da tela pulsa pelo relogio, e dois
    // quadros DIRETOS seguidos ja diferem ali. A mascara e a uniao do que
    // muda entre seis quadros diretos.
    { unsigned char *ant = NULL; int q, v;
      mascara = calloc(1920 * 1080, 1);
      assert(mascara);
      for (v = 0; v < 6; v++) {
        unsigned char *c;
        quadro(w); c = ler(); SDL_GL_SwapWindow(w);
        if (ant) for (q = 0; q < 1920 * 1080; q++) {
          int k2;
          for (k2 = 0; k2 < 3; k2++)
            if (abs(c[q * 4 + k2] - ant[q * 4 + k2]) > 2) mascara[q] = 1;
        }
        free(ant); ant = c;
      }
      free(ant);
      // A onda do esqueleto anda: seis quadros nao cobrem o percurso inteiro.
      // A mascara vira o RETANGULO do que mudou, com 16 px de folga.
      { int x0 = 1920, y0 = 1080, x1 = -1, y1 = -1, x, y;
        for (q = 0; q < 1920 * 1080; q++) if (mascara[q]) {
          x = q % 1920; y = q / 1920;
          if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; if (y > y1) y1 = y; }
        if (x1 >= 0) {
          x0 = x0 > 16 ? x0 - 16 : 0; y0 = y0 > 16 ? y0 - 16 : 0;
          x1 = x1 + 16 < 1919 ? x1 + 16 : 1919; y1 = y1 + 16 < 1079 ? y1 + 16 : 1079;
          for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) mascara[y * 1920 + x] = 1;
        } }
      for (q = 0, v = 0; q < 1920 * 1080; q++) v += mascara[q];
      printf("  a home anima %d pixels sozinha (fora da comparacao)\n", v);
      confere("o que anima sozinho e menos de 10 por cento da tela", v < 1920 * 1080 / 10, 1); }
    podeParar = 1;
    f0 = spainel_n_fundos();
    quadro(w); a = ler(); SDL_GL_SwapWindow(w);
    confere("o quadro seguinte pinta a copia", spainel_n_fundos() - f0, 1);
    quadro(w); SDL_GL_SwapWindow(w);   // um quadro so copiando
    podeParar = 0;
    quadro(w); b = ler(); SDL_GL_SwapWindow(w);
    podeParar = 1;
    for (k = 0; k < 1920 * 1080 * 4; k++) {
      int d = a[k] > b[k] ? a[k] - b[k] : b[k] - a[k];
      if ((k & 3) == 3) continue;   // alfa do alvo: nao aparece
      if (mascara[k / 4]) continue;
      if (d > pior) pior = d;
      if (d > 2) difs++;
    }
    printf("  pior diferenca de canal: %d, pixels acima de 2: %d\n", pior, difs);
    confere("copia parada igual ao desenho direto (tolerancia 2)", difs, 0);
    free(a); free(b); free(mascara); }
  f0 = spainel_n_fundos();
  quadros(w, 5);
  confere("depois do quadro direto a copia e refeita uma vez", spainel_n_fundos() - f0, 1);
  f0 = spainel_n_fundos();
  montar(3);   // a descoberta republicou: fileiras novas por baixo
  quadros(w, 5);
  confere("catalogo trocado: copia refeita uma vez", spainel_n_fundos() - f0, 1);
  f0 = spainel_n_fundos();
  spainel_fechar();
  quadros(w, 30);
  spainel_abrir();
  quadros(w, 5);
  confere("entrando: o fundo ainda e desenhado direto", spainel_n_fundos() - f0, 0);
  quadros(w, 30);
  confere("entrada terminada: pintado de novo", spainel_n_fundos() - f0, 1);

  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return falhas ? 1 : 0;
}
