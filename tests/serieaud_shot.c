// CAPTURA DOS TRES PAINEIS DE AUDIENCIA, sem rede e sem interacao.
//
// Existe pelo motivo que tests/ajustes_shot.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Grafico e pior ainda — o defeito
// tipico (rotulo pequeno demais, ponto por cima de ponto, curva que some contra
// o fundo) nao aparece em teste de logica nenhum.
//
// OS NUMEROS SAO REAIS, medidos na api.trakt.tv em 16/09/2026. Nao ha dado de
// mentira aqui de proposito: uma curva inventada e sempre bonita, e a pergunta
// que a captura responde e se a curva DE VERDADE le bem.
//
// Dois conjuntos, porque desenham caminhos diferentes:
//   breaking-bad T2  — temporada inteira, retencao quase plana (97% no fim).
//   deep-space-nine T1 — o E2 tem MAIS gente que o E1 (retencao > 100%), e dois
//     episodios entram SEM /stats para mostrar como o buraco e desenhado.
//
// NAO CHAMA dados_iniciar DE PROPOSITO: sem ela dados_dir() e "" e o modulo nao
// le nem escreve arquivo nenhum — a captura nao encosta no cache de quem roda.
#include "serieaud.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/serieaud.c"

typedef struct { int ep, nota; long w, p; int com, vot; } Fix;

// breaking-bad, temporada 2. notas de /seasons/2?extended=full e o resto de
// /seasons/2/episodes/<n>/stats.
static const Fix BB[] = {
  {  1, 81, 364060, 423111, 10, 3445 },
  {  2, 84, 361321, 418989, 13, 3467 },
  {  3, 79, 360700, 417529,  8, 3278 },
  {  4, 78, 358789, 416703, 20, 3236 },
  {  5, 79, 358206, 415361, 14, 3224 },
  {  6, 81, 356977, 413054, 23, 3232 },
  {  7, 80, 357109, 413444, 15, 3203 },
  {  8, 84, 355875, 412309, 22, 3254 },
  {  9, 83, 354997, 411444, 21, 3201 },
  { 10, 80, 354231, 409979, 22, 3110 },
  { 11, 83, 353772, 408180, 13, 3131 },
  { 12, 85, 353026, 406882, 28, 3147 },
  { 13, 85, 353462, 407430, 20, 3171 }
};

// star-trek-deep-space-nine, temporada 1. Os episodios 5 e 9 entram com
// watchers = 0: e o estado "o /stats nao respondeu", que TEM de ser desenhado
// como buraco e nunca como zero espectadores.
static const Fix DS9[] = {
  {  1, 76, 25164, 38252, 13, 715 },
  {  2, 73, 25245, 31669,  4, 574 },
  {  3, 73, 24902, 31149,  4, 539 },
  {  4, 74, 24522, 30661,  8, 538 },
  {  5, 75,     0,     0,  0,   0 },
  {  6, 72, 24263, 30302,  6, 527 },
  {  7, 73, 23998, 29809,  6, 495 },
  {  8, 68, 23819, 29587,  5, 481 },
  {  9,  0,     0,     0,  0,   0 },
  { 10, 72, 23624, 29104,  5, 484 },
  { 11, 72, 23622, 29050,  6, 471 },
  { 12, 71, 23465, 28961,  3, 477 }
};

// --- FORMAS DE DADO QUE NAO SAO A FELIZ ---------------------------------------
//
// As duas temporadas acima sao reais e bem comportadas. O que quebra grafico
// nao e o caso bonito: e a temporada estreando, a que desaba, a de 24
// episodios e a de 3. Estas quatro sao SINTETICAS e estao marcadas como tal —
// os numeros nao valem para nada alem de exercitar o desenho.

// ESTREANDO: seis episodios no ar e o setimo lancado ontem, com 4% das
// marcacoes do E1. E o caso que achatava o eixo inteiro — o menor valor
// puxava o piso a 0 e a temporada virava uma reta colada no teto.
static const Fix ESTREIA[] = {
  {  1, 82, 41000, 44200, 31, 980 },
  {  2, 80, 39800, 42500, 22, 910 },
  {  3, 83, 39100, 41600, 26, 903 },
  {  4, 79, 38400, 40900, 18, 871 },
  {  5, 84, 38050, 40500, 29, 884 },
  {  6, 81, 37600, 39800, 20, 842 },
  {  7, 86,  1620,  1690, 44, 120 }
};

// DESABANDO de verdade: metade do publico sai ate o fim. Aqui NENHUM ponto e
// discrepante e a escala tem de mostrar a queda inteira, sem excluir nada.
static const Fix QUEDA[] = {
  {  1, 74, 88000, 96000, 40, 2100 },
  {  2, 71, 79000, 84000, 22, 1800 },
  {  3, 66, 69000, 72000, 18, 1500 },
  {  4, 62, 61000, 63000, 12, 1220 },
  {  5, 58, 54000, 55500,  9,  980 },
  {  6, 55, 48000, 49000,  7,  830 },
  {  7, 52, 44000, 44800,  6,  760 },
  {  8, 61, 43000, 44000, 14,  910 }
};

// SUBINDO, e com tres episodios so: o minimo que faz curva. Serve para ver se
// o eixo de episodios e os rotulos de extremo aguentam n = 3.
static const Fix CURTA[] = {
  {  1, 68, 12400, 13000,  5,  310 },
  {  2, 74, 12250, 13100,  9,  344 },
  {  3, 83, 12300, 14400, 21,  402 }
};

// VINTE E QUATRO episodios com buracos: o teto de SA_EP_MAX, que e onde os
// rotulos e as barras ficam mais apertados. Tres episodios entram sem /stats e
// dois sem nota.
static const Fix LONGA[] = {
  {  1, 78, 52000, 57000, 20, 1400 }, {  2, 76, 50800, 54000, 12, 1290 },
  {  3, 81, 50100, 53200, 18, 1330 }, {  4,  0, 49600, 52400,  9, 1210 },
  {  5, 74,     0,     0,  0,    0 }, {  6, 79, 48700, 51300, 11, 1180 },
  {  7, 83, 48200, 51000, 24, 1260 }, {  8, 77, 47800, 50100,  8, 1120 },
  {  9, 75, 47400, 49600,  7, 1090 }, { 10, 80, 47100, 49500, 15, 1150 },
  { 11, 72,     0,     0,  0,    0 }, { 12, 84, 46400, 49200, 28, 1240 },
  { 13, 79, 46100, 48300, 13, 1100 }, { 14, 76, 45800, 47900,  9, 1040 },
  { 15,  0, 45500, 47500,  6,  990 }, { 16, 82, 45200, 47600, 19, 1130 },
  { 17, 78, 44900, 46900, 10, 1020 }, { 18, 73, 44600, 46300,  5,  960 },
  { 19, 81, 44300, 46500, 16, 1080 }, { 20, 85, 44000, 46800, 31, 1190 },
  { 21, 77,     0,     0,  0,    0 }, { 22, 79, 43400, 45400, 12, 1010 },
  { 23, 83, 43100, 45600, 22, 1120 }, { 24, 88, 43000, 46900, 39, 1310 }
};

static void carregar(const Fix *f, int n, const char *imdb, int temp,
                     long plSerie, long wtSerie, int sel) {
  int i;
  memset(eps, 0, sizeof eps);
  for (i = 0; i < n && i < SA_EP_MAX; i++) {
    eps[i].ep = f[i].ep;
    eps[i].nota = f[i].nota;
    if (f[i].w > 0) {
      eps[i].tem = 1;
      eps[i].watchers = f[i].w;
      eps[i].plays = f[i].p;
      eps[i].comentarios = f[i].com;
      eps[i].votos = f[i].vot;
    }
  }
  nEps = i;
  temporadaAtual = temp;
  snprintf(imdbAtual, sizeof imdbAtual, "%s", imdb);
  playsSerie = plSerie;
  watchersSerie = wtSerie;
  selecionado = sel;
  truncada = 0;
}

static void gravar(const char *nome, SDL_Window *win) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  SDL_GL_SwapWindow(win);
  printf("captura: %s  (%d desenhos gfx no quadro)\n", nome, gfx_n_rect);
}

// Um quadro so nao basta: text.c rasteriza no maximo TXT_POR_QUADRO linhas por
// quadro (o teto existe para nao engasgar a TV ao abrir tela), entao a primeira
// passada sai com quase todo o texto faltando. Repetir ate o cache encher e
// exatamente o que o aparelho faz nos primeiros quadros da secao.
// O CHAO DE VERDADE DESTA PAGINA, e nao um preto chapado.
//
// A captura limpava o quadro com NV_COR_FUNDO e desenhava os paineis por cima.
// Isso NAO e o que a TV mostra: detail.c desenha estas secoes sobre o BACKDROP
// DA OBRA APAGADO A 15% (medido na folha do web, `.detail-scrolled` leva o
// backdrop a opacity 0.15), e e por isso que uma caixa de cor chapada atras do
// grafico aparece la como uma placa colada por cima da arte — defeito que a
// captura em preto chapado nao mostrava, porque nao havia arte para tapar.
//
// Entao a captura agora pinta o mesmo chao: a mesma arte, o mesmo GFX_DETALHE,
// o mesmo 0.15. Sem isto nao da para julgar "fundo mais transparente".
static GLuint fundoTex;
static const char *fundoArte;

static void chaoDaPagina(void) {
  GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
  if (!fundoTex) return;
  gfx_tex_aspect_atual = tex_aspecto(fundoArte);
  // 4o parametro = forca da vinheta; 0 = pagina ja rolada, que e o estado em
  // que estas secoes sao vistas. alpha 0.15 = 1.0 * (1 - 0.85), a conta de
  // detail.c com pg = 1.
  gfx_rect(tela, fundoTex, GFX_DETALHE, 0.0f, 0, 0, 0.0f, 0, 0, 0, 0.15f);
  gfx_tex_aspect_atual = 0.0f;
}

static void desenharVarias(void (*f)(void), SDL_Window *win) {
  int i;
  for (i = 0; i < 40; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    chaoDaPagina();
    f();
    if (i < 39) SDL_GL_SwapWindow(win);
  }
  (void)win;
}

static void telaArcoRadar(void) {
  GfxRect a = { NV_CONTENT_PAD, 70.0f, NV_TELA_W - NV_CONTENT_PAD * 2, 0 };
  GfxRect b = a;
  b.y = 560.0f;
  serieaud_arco(a);
  serieaud_radar(b);
}

static void telaDigital(void) {
  GfxRect d = { NV_CONTENT_PAD, 90.0f, NV_TELA_W - NV_CONTENT_PAD * 2, 0 };
  serieaud_digital(d);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-serieaud";
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: audiencia da serie", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);
  // Uma arte qualquer do pacote serve: o que se testa e se o grafico convive
  // com um chao NAO UNIFORME e um pouco mais claro que o preto, nao qual obra.
  fundoArte = "deploy/app/art/00.jpg";
  fundoTex = tex_obter_hero(fundoArte);
  { int n; for (n = 0; n < 200 && !fundoTex; n++) { tex_bombear(8);
      fundoTex = tex_obter_hero(fundoArte); SDL_Delay(5); } }
  printf("chao da pagina: %s (tex %u)\n", fundoArte, (unsigned)fundoTex);

  // breaking-bad T2: 24126034 reproducoes / 404730 espectadores da serie.
  carregar(BB, (int)(sizeof BB / sizeof BB[0]), "tt0903747", 2,
           24126034L, 404730L, 7);
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-bb.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-bb-digital.bmp", saida);
  gravar(nome, w);

  // deep-space-nine T1: 4365144 / 29405.
  carregar(DS9, (int)(sizeof DS9 / sizeof DS9[0]), "tt0106145", 1,
           4365144L, 29405L, 1);
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-ds9.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-ds9-digital.bmp", saida);
  gravar(nome, w);

  // As quatro formas sinteticas. O comentario de cada vetor diz o que ela
  // exercita; a captura e a unica prova de que exercita mesmo.
  carregar(ESTREIA, (int)(sizeof ESTREIA / sizeof ESTREIA[0]), "tt9000001", 1,
           240000L, 41000L, 6);
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-estreia.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-estreia-digital.bmp", saida);
  gravar(nome, w);

  carregar(QUEDA, (int)(sizeof QUEDA / sizeof QUEDA[0]), "tt9000002", 1,
           520000L, 91000L, 3);
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-queda.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-queda-digital.bmp", saida);
  gravar(nome, w);

  carregar(CURTA, (int)(sizeof CURTA / sizeof CURTA[0]), "tt9000003", 1,
           39000L, 12500L, 2);
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-curta.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-curta-digital.bmp", saida);
  gravar(nome, w);

  // A temporada de 24 entra com `truncada` levantada: e o estado real de quem
  // tem mais episodios que o teto de pedidos, e o rodape tem de dizer isso.
  carregar(LONGA, (int)(sizeof LONGA / sizeof LONGA[0]), "tt9000004", 1,
           3100000L, 53000L, 11);
  truncada = 1;
  desenharVarias(telaArcoRadar, w);
  snprintf(nome, sizeof nome, "%s-longa.bmp", saida);
  gravar(nome, w);
  desenharVarias(telaDigital, w);
  snprintf(nome, sizeof nome, "%s-longa-digital.bmp", saida);
  gravar(nome, w);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  return 0;
}
