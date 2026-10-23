// O FUNDO DO PAINEL DE SALVOS/SOCIAL COM O TEMPO (relato do dono, 03/10:
// "quando a sidebar social ta aberta, depois de um tempo a home some e so
// volta quando eu fecho").
//
// Com o painel parado, a home de tras e pintada uma vez no FBO do snapshot
// (spainel_fundo) e os quadros seguintes so copiam. Este teste abre o painel,
// espera a copia assentar, e mede o brilho medio da faixa da home que fica a
// ESQUERDA do painel (fora dele, so com o veu por cima). Depois provoca o que
// acontece "depois de um tempo" numa TV: outra arte entrando no cache de
// texturas (capas e rostos do painel, fileiras novas) e uma republicacao do
// catalogo (Continuar assistindo refeito, sync), e mede de novo.
//
//   bash tests/spainel_fundo_tempo.sh
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

#define NFIL   6
#define PORFIL 20
#define NCAT   (NFIL * PORFIL)

static SDL_Window *janela;
static CatItem itens[NCAT];
static CatFileira fils[NFIL];
static void homeFundo(void *ctx) { (void)ctx; home_desenhar(SDL_GetTicks()); }

// Pressao no cache: `pressao` caminhos novos por quadro, como as capas e os
// rostos do painel e a arte de fileiras que chegam enquanto ele esta aberto.
static int pressao, pressaoSeq, parar = 1;
static void pressionar(void) {
  int k;
  for (k = 0; k < pressao; k++, pressaoSeq++) {
    char c[160];
    int r, o = snprintf(c, sizeof c, "deploy/app/art/");
    // Caminho distinto, mesmo arquivo: para o cache e arte nova.
    for (r = 0; r < (pressaoSeq / 40) % 50; r++) o += snprintf(c + o, sizeof c - (size_t)o, "./");
    snprintf(c + o, sizeof c - (size_t)o, "%02d.jpg", pressaoSeq % 40);
    tex_obter_larg(c, 300.0f);
  }
}

static void quadro(void) {
  float dt = 1.0f / 60.0f;
  SDL_PumpEvents();
  tex_bombear(3);
  home_painel_aberto(spainel_aberto());   // como app.c, antes de atualizar a home
  home_atualizar(dt, SDL_GetTicks());
  spainel_atualizar(dt, SDL_GetTicks());
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
  spainel_fundo(parar, cat_revisao(), homeFundo, NULL);   // o caminho de app.c
  spainel_desenhar(SDL_GetTicks());
  pressionar();
  glFinish();
  SDL_GL_SwapWindow(janela);
}

static void rodarMs(Uint32 ms) {
  Uint32 ate = SDL_GetTicks() + ms;
  while (SDL_GetTicks() < ate) { quadro(); SDL_Delay(4); }
}

// Brilho medio (0..255) e fracao de pixels "com conteudo" (acima do fundo
// vazio + veu) na faixa da home a esquerda do painel, do quadro que acabou de
// ser desenhado (back buffer, antes da troca: refaz um quadro sem trocar).
static unsigned char pix[1000 * 1080 * 4], ref[1000 * 1080 * 4];
static double rodapeCheio;   // fracao do rodape (100 px de baixo) com conteudo
static double textoHero;     // fracao da faixa do titulo do destaque com texto claro
static void medir(const char *rotulo, double *media, double *cheia) {
  long soma = 0, acima = 0, n = 0, rod = 0;
  int i;
  float dt = 1.0f / 60.0f;
  tex_bombear(3);
  home_painel_aberto(spainel_aberto());   // como app.c, antes de atualizar a home
  home_atualizar(dt, SDL_GetTicks());
  spainel_atualizar(dt, SDL_GetTicks());
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
  spainel_fundo(parar, cat_revisao(), homeFundo, NULL);
  spainel_desenhar(SDL_GetTicks());
  glFinish();
  glReadPixels(0, 0, 1000, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (i = 0; i < 1000 * 1080; i++) {
    int l = (pix[i * 4] + pix[i * 4 + 1] + pix[i * 4 + 2]) / 3;
    soma += l; n++;
    if (l > 14) { acima++; if (i < 1000 * 100) rod++; }
  }
  rodapeCheio = rod / (1000.0 * 100.0);
  // O TEXTO DO DESTAQUE (titulo, meta, botao: y 560..960 do topo, x 100..420):
  // cinza claro sobre a arte escurecida pelo veu. Sem o destaque a faixa so tem
  // fundo/brilho vermelho (< 90).
  { long claro = 0; int y, x;
    for (y = 560; y < 960; y++) for (x = 100; x < 420; x++) {
      long o = ((long)(1079 - y) * 1000 + x) * 4;
      if ((pix[o] + pix[o + 1] + pix[o + 2]) / 3 > 90) claro++;
    }
    textoHero = claro / (320.0 * 400.0); }
  if (getenv("SPFT_PPM")) {
    static int nppm; char nome[600]; FILE *f; int y;
    snprintf(nome, sizeof nome, "%s-%d.ppm", getenv("SPFT_PPM"), nppm++);
    f = fopen(nome, "wb");
    if (f) {
      fprintf(f, "P6 1000 1080 255\n");
      for (y = 1079; y >= 0; y--) for (i = 0; i < 1000; i++) fwrite(pix + (y * 1000 + i) * 4, 1, 3, f);
      fclose(f);
    }
  }
  SDL_GL_SwapWindow(janela);
  *media = (double)soma / n;
  *cheia = (double)acima / n;
  { int it = 0, pend = 0, q = 0; long b = 0, bq = 0;
    tex_estatisticas(&it, &pend, &b, &q, &bq);
    printf("%-34s brilho=%.2f conteudo=%.1f%% | tex itens=%d despejos=%d quentes=%d fundos=%d\n",
           rotulo, *media, *cheia * 100.0, it, tex_despejos, q, spainel_n_fundos()); }
}

// A ARTE DO DESTAQUE (o alto da faixa, sem fileiras nem texto de fileira): guarda
// o ultimo quadro medido e conta quantos pixels dela mudaram desde `guardar`.
static unsigned char arteRef[1000 * 540 * 4];
static void guardarArte(void) {
  int y;
  for (y = 0; y < 540; y++) memcpy(arteRef + (size_t)y * 4000, pix + (size_t)(1079 - y) * 4000, 4000);
}
static double arteMudou(void) {
  long dif = 0; int y, x;
  for (y = 0; y < 540; y++) for (x = 0; x < 1000; x++) {
    const unsigned char *a = arteRef + ((size_t)y * 1000 + x) * 4, *b = pix + ((size_t)(1079 - y) * 1000 + x) * 4;
    if (abs((a[0] + a[1] + a[2]) / 3 - (b[0] + b[1] + b[2]) / 3) > 12) dif++;
  }
  return dif / (1000.0 * 540.0);
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.state = SDL_PRESSED;
  home_evento(&e);
  e.type = SDL_KEYUP; e.key.state = SDL_RELEASED;
  home_evento(&e);
}

static void povoar(void) {
  int f, i;
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (f = 0; f < NFIL; f++) {
    snprintf(fils[f].chave, sizeof fils[f].chave, f ? "t_movie_%02d" : "continue_watching", f);
    snprintf(fils[f].titulo, sizeof fils[f].titulo, f ? "Fileira %d" : "Continuar assistindo", f);
    snprintf(fils[f].tipo, sizeof fils[f].tipo, "movie");
    fils[f].ini = f * PORFIL;
    fils[f].n = PORFIL;
    for (i = 0; i < PORFIL; i++) {
      int k = f * PORFIL + i, t = k % 40;
      CatItem *c = &itens[k];
      snprintf(c->imdb, sizeof c->imdb, "tt%07d", 1000000 + k);
      snprintf(c->tipo, sizeof c->tipo, "movie");
      snprintf(c->titulo, sizeof c->titulo, "Titulo %d", k);
      snprintf(c->meta, sizeof c->meta, "%d", 1990 + t);
      snprintf(c->genero, sizeof c->genero, "Filme");
      snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", t);
      snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (t + 7) % 40);
      if (f == 0) { c->progresso = 30; c->restanteMin = 20; }
    }
  }
  cat_definir_tudo(itens, NCAT, fils, NFIL);
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  SDL_GLContext gl;
  double b0, c0, b1, c1, b2, c2, b3, c3, tituloAssentado, t1, t2, t3;
  int falhas = 0;
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("dados_dir() != NUVIO_DADOS; recusando\n"); return 2; }
  { char caminho[700]; FILE *f;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma 0\n"); fclose(f);
    ajustes_dir(dir); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("spainel fundo tempo", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  gfx_snap_iniciar((int)NV_TELA_W, (int)NV_TELA_H);
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(getenv("SLOTS") ? atoi(getenv("SLOTS")) : 64);
  gfx_icones_dir("deploy/app/art");
  assert(home_iniciar("deploy/app/art"));
  salvos_iniciar();
  povoar();

  rodarMs(1500);                       // home assentada, arte no cache
  spainel_abrir();
  // O QUE ACABOU DE SER VISTO (a fileira Continuar assistindo, nada salvo)
  // aparece em "Continuar" na aba Salvos, com a barra (dono, 03/10).
  { int nc = spainel_n_continuar();
    printf("aba Salvos: %d de %d da fileira Continuar assistindo em \"Continuar\"\n", nc, PORFIL);
    if (nc != PORFIL) { printf("FALHA: o que acabou de ser visto nao esta no painel\n"); falhas++; } }
  rodarMs(5500);                       // entrada + as tres repinturas (0,4/1,5/4 s)
  medir("painel aberto, assentado", &b0, &c0);
  // A repintura de 4 s vem ~2,5 s depois da de 1,5 s: a home nao e desenhada
  // nesse intervalo e lia a ausencia como "voltou de outra tela", recomecando a
  // cascata das fileiras no quadro que ficava congelado (relato do dono, 13/10:
  // "a sidebar ainda esconde a home atras depois de um tempo").
  printf("rodape assentado: %.1f%% com conteudo; titulo do destaque: %.2f%%\n", rodapeCheio * 100.0, textoHero * 100.0);
  if (rodapeCheio < 0.2) { printf("FALHA: depois da ultima repintura a copia ficou sem as fileiras\n"); falhas++; }

  tituloAssentado = textoHero;
  if (textoHero < 0.01) { printf("FALHA: a copia assentada ficou sem o titulo do destaque\n"); falhas++; }

  // 1) So o tempo passando, nada muda: a copia tem de continuar igual.
  rodarMs(3000);
  medir("+3 s parado", &b1, &c1);
  t1 = textoHero;

  // 2) Arte nova entrando no cache com o painel aberto (capas, rostos).
  pressao = getenv("PRESSAO") ? atoi(getenv("PRESSAO")) : 4;
  rodarMs(3000);
  pressao = 0;
  medir("+3 s com arte nova no cache", &b2, &c2);
  t2 = textoHero;

  // 3) Republicacao do catalogo (Continuar assistindo refeito, sync): a
  // revisao sobe e a copia e repintada.
  cat_trocar_continuar(itens, PORFIL);
  rodarMs(3000);
  medir("+3 s depois de republicar", &b3, &c3);
  t3 = textoHero;
  memcpy(ref, pix, sizeof pix);

  // A REFERENCIA: a mesma home desenhada direto (sem a copia parada), com
  // tempo para a arte dela voltar ao cache. A copia tem de bater com ela.
  { double bd, cd; long dif = 0; int k;
    parar = 0;
    rodarMs(1500);
    medir("referencia: home desenhada direto", &bd, &cd);
    for (k = 0; k < 1000 * 1080; k++) {
      int a = (ref[k * 4] + ref[k * 4 + 1] + ref[k * 4 + 2]) / 3;
      int b = (pix[k * 4] + pix[k * 4 + 1] + pix[k * 4 + 2]) / 3;
      if (abs(a - b) > 12) dif++;
    }
    printf("copia parada x desenho direto: %.1f%% dos pixels diferem\n", dif * 100.0 / (1000 * 1080));
    // O RODAPE DA HOME (a fileira que espia sob o destaque): a copia parada
    // tem de ter os cartazes que o desenho direto tem. 5 % dos pixels
    // diferentes (acima) nao pega 80 px de cartaz em branco.
    { long sa = 0, sb = 0; int y;
      for (y = 0; y < 100; y++) for (k = 0; k < 1000; k++) {
        long o = ((long)y * 1000 + k) * 4;
        sa += (ref[o] + ref[o + 1] + ref[o + 2]) / 3;
        sb += (pix[o] + pix[o + 1] + pix[o + 2]) / 3;
      }
      printf("rodape da home (copia x direto): brilho %.2f x %.2f\n", sa / 100000.0, sb / 100000.0);
      if (sa < sb * 0.9) { printf("FALHA: a copia parada ficou sem os cartazes do rodape da home\n"); falhas++; } }
    // O DESTAQUE COM O TEMPO (relato do dono, 13/10: a TCL ficou sem fileiras E
    // sem destaque). O carrossel girava com a home parada, e a troca e
    // efetivada no DESENHO: a repintura seguinte pegava o quadro em que o texto
    // do titulo novo ainda esta em opacidade 0 (heroEntra) e a copia o guardava
    // assim. O titulo tem de estar na copia em todas as medidas.
    printf("titulo do destaque na copia: assentada %.2f%%, +3 s %.2f%%, arte nova %.2f%%, republicada %.2f%%; direto %.2f%%\n",
           tituloAssentado * 100.0, t1 * 100.0, t2 * 100.0, t3 * 100.0, textoHero * 100.0);
    if (t1 < 0.01 || t2 < 0.01 || t3 < 0.01) { printf("FALHA: o destaque sumiu da copia parada com o tempo\n"); falhas++; }
    if (b1 != b0 || c1 != c0) { printf("FALHA: a copia mudou so com o tempo\n"); falhas++; }
    if (c2 < c0 * 0.8) { printf("FALHA: a home sumiu com arte nova no cache\n"); falhas++; }
    if (dif > 1000 * 1080 / 20 || b3 < bd * 0.85) { printf("FALHA: depois de republicar a copia nao e a home (sumiu)\n"); falhas++; } }
  // 4) O CARROSSEL DO DESTAQUE GIRANDO ATRAS DA COPIA (relato do dono, 13/10: a
  // TCL ficou sem fileiras E sem destaque). A troca e efetivada no DESENHO, que
  // com a copia parada so acontece nas repinturas: passados os 7 s, a primeira
  // repintura pegava o destaque no quadro inicial da troca (texto em opacidade
  // 0) e, sem arte faltando, nao havia outra para refazer: a copia ficava
  // assim ate fechar o painel.
  { double bt, ct;
    int n, pior = 0;
    parar = 1;
    pressao = getenv("PRESSAO") ? atoi(getenv("PRESSAO")) : 4;
    // Republica de 5,3 em 5,3 s: a fase entre a republicacao (que zera as
    // repinturas por tempo 0,4/1,5/4 s) e o vencimento dos 7 s do carrossel
    // varre todo o intervalo; em alguma delas a ultima repintura pega a troca.
    medir("copia assentada de novo", &bt, &ct);
    guardarArte();
    for (n = 0; n < (getenv("SPFT_SO_FECHAR") ? 0 : 8); n++) {   // SPFT_SO_FECHAR: depura so o cenario 5
      double mudou;
      cat_trocar_continuar(itens, PORFIL);
      rodarMs(5300);
      medir("republicada, copia assentada", &bt, &ct);
      mudou = arteMudou();
      printf("  texto do destaque %.2f%%, arte do destaque mudou %.1f%%\n", textoHero * 100.0, mudou * 100.0);
      // Com o painel aberto o destaque fica onde estava: girar por tras da
      // copia e o que a deixava no quadro inicial da troca.
      if (textoHero < 0.01 || mudou > 0.05) pior++;
    }
    pressao = 0;
    if (pior) { printf("FALHA: o destaque girou/sumiu atras do painel em %d de 8 republicacoes (carrossel atras do painel)\n", pior); falhas++; }

    // 5) FECHAR O PAINEL E O CARROSSEL VOLTAR. Quem voltou um titulo a mao
    // (seta esquerda) desligou o carrossel; fechar o painel nao recomeca a
    // cascata das fileiras (a copia manteve as fileiras "vistas"), e era so a
    // cascata que o religava. Sem isso o destaque ficava parado ate sair da home.
    spainel_fechar();
    rodarMs(1500);
    tecla(SDLK_RIGHT); rodarMs(1500);
    tecla(SDLK_RIGHT); rodarMs(1500);
    tecla(SDLK_LEFT);  rodarMs(1500);            // heroAutoDesligado = 1
    spainel_abrir();
    rodarMs(3000);
    spainel_fechar();
    rodarMs(1500);
    medir("painel fechado, carrossel religado", &bt, &ct);
    guardarArte();
    rodarMs(14000);                              // ocio de 12 s da home + o intervalo
    medir("14 s depois", &bt, &ct);
    { double mudou = arteMudou();
      printf("arte do destaque 14 s depois de fechar o painel: mudou %.1f%%\n", mudou * 100.0);
      if (mudou < 0.05) { printf("FALHA: o carrossel do destaque nao voltou depois de fechar o painel\n"); falhas++; } } }
  printf(falhas ? "spainel_fundo_tempo: %d falha(s)\n" : "spainel_fundo_tempo: ok%.0d\n", falhas);
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return falhas ? 1 : 0;
}
