// CABECALHO DA COLECAO, DIRECAO A (job 205-colhdrlinha), em PNG + geometria.
//
//   zero.png      Linha do tempo, 0 de 9 vistos: so a contagem, nenhum ponto.
//   parcial.png   Cover Flow, 7 de 23: anel de 23 pontos.
//   completo.png  Ranking, 11 de 11: anel no acento + check.
//   foco.png      Decadas, 30 de 40, foco no seletor: anel no teto de 24.
//   vitrine.png   Vitrine: sobretitulo e nome NA MESMA LINHA (a do relogio).
//
// Confere sem ler pixel: nas exibicoes de grade o nome fica SOB o sobretitulo;
// na Vitrine ao lado, na mesma linha; o seletor termina na borda da ilha do
// relogio. Sai 1 se algo falha. Nao entra na suite: precisa de janela GL.
static int shotN, shotVistos;
#define desc_vertudo_n shot_vertudo_n
#define desc_vertudo_item shot_vertudo_item
#define vistocont_contar shot_vistocont_contar
#define desc_vertudo_n_bruto shot_vertudo_n
#define desc_vertudo_item_bruto shot_vertudo_item
#include "../src/vertudo.c"
#include "nvtmp.h"
#undef desc_vertudo_n
#undef desc_vertudo_item
#undef vistocont_contar
#undef desc_vertudo_n_bruto
#undef desc_vertudo_item_bruto
int desc_vertudo_n(void);
int desc_vertudo_item(int i, CatItem *dst);
int shot_vertudo_n(void) { return shotN; }
int shot_vertudo_item(int i, CatItem *dst) {
  static const char *nomes[] = { "Reservoir Dogs", "Pulp Fiction", "Jackie Brown", "Kill Bill: Vol. 1",
                                 "Kill Bill: Vol. 2", "Death Proof", "Inglourious Basterds", "Django Unchained",
                                 "The Hateful Eight", "Once Upon a Time in Hollywood", "Four Rooms", "Sin City" };
  if (i < 0 || i >= shotN || !dst) return 0;
  memset(dst, 0, sizeof *dst);
  snprintf(dst->imdb, sizeof dst->imdb, "tt%07d", 100 + i);
  snprintf(dst->tipo, sizeof dst->tipo, "movie");
  snprintf(dst->titulo, sizeof dst->titulo, "%s", nomes[i % 12]);
  snprintf(dst->genero, sizeof dst->genero, "Movie · Crime · Thriller");
  snprintf(dst->meta, sizeof dst->meta, "%d · 120 min", 1992 + i);
  snprintf(dst->sinopse, sizeof dst->sinopse, "A story told out of order.");
  snprintf(dst->poster, sizeof dst->poster, "deploy/app/art/poster/%02d.jpg", i % 40);
  snprintf(dst->backdrop, sizeof dst->backdrop, "deploy/app/art/%02d.jpg", i % 30);
  return 1;
}
int shot_vistocont_contar(const VcEntrada *e, int n, int *vistos) {
  (void)e;
  *vistos = shotVistos < n ? shotVistos : n;
  return n;
}
#include <SDL2/SDL_image.h>
#include <assert.h>

static ColFolder pasta;
static ColSource pastaFontes[COL_SOURCE_MAX];
static int falhas;

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    vertudo_atualizar(1.0f / 60.0f, SDL_GetTicks());
    vertudo_desenhar(SDL_GetTicks());
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(IMG_SavePNG(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

static void confere(int ok, const char *estado, const char *o_que) {
  printf("%s %s: %s\n", ok ? "PASS" : "FAIL", estado, o_que);
  falhas += !ok;
}

static void estado(int l, int n, int vistos, int cab, const char *saida, const char *nome,
                   int pontos, SDL_Window *w) {
  char cam[600];
  shotN = n; shotVistos = vistos;
  layEscolha = l; resolverLayout(); celLay = -1; vistosEm = 0;
  if (l == VT_LAY_VITRINE) vitIniciar();
  foco = 0; tabFocus = cab; tabCursor = cab ? cabN() : 0;
  fluxoPos = tlProg = -1.0f;
  cabEb = cabTt = cabSw = (GfxRect){ 0 }; cabPontos = -1;
  snprintf(cam, sizeof cam, "%s/%s.png", saida, nome);
  captura(cam, w);
  if (l == VT_LAY_VITRINE) {
    confere(fabsf((cabEb.y + cabEb.h) - (cabTt.y + cabTt.h)) < 14.0f && cabTt.x > cabEb.x + cabEb.w,
            nome, "eyebrow and title share one row");
    confere(cabTt.y < NV_ILHA_Y + NV_ILHA_H, nome, "title on the clock row");
    return;
  }
  confere(cabEb.w > 0 && cabTt.y >= cabEb.y + cabEb.h, nome, "title below eyebrow");
  confere(cabPontos == pontos, nome, "ring dots");
  confere(fabsf(cabSw.x + cabSw.w - (NV_TELA_W - NV_ILHA_MARGEM_D)) < 1.0f && cabSw.y > NV_ILHA_Y + NV_ILHA_H &&
          cabSw.y + cabSw.h < VT_TOPO, nome, "switcher on clock edge, above the grid");
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-colhdr");
  SDL_Window *w;
  SDL_GLContext gl;
  pasta.sources = pastaFontes;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: cabecalho", SDL_WINDOWPOS_CENTERED,
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
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  snprintf(pasta.title, sizeof pasta.title, "Quentin Tarantino");
  snprintf(pasta.group, sizeof pasta.group, "Directors");
  snprintf(pastaFontes[0].title, sizeof pastaFontes[0].title, "Quentin Tarantino");
  snprintf(pastaFontes[0].type, sizeof pastaFontes[0].type, "movie");
  pasta.nSources = 1;
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0;

  estado(VT_LAY_LINHA, 9, 0, 0, saida, "zero", 0, w);
  estado(VT_LAY_FLUXO, 23, 7, 0, saida, "parcial", 23, w);
  estado(VT_LAY_RANKING, 11, 11, 0, saida, "completo", 11, w);
  estado(VT_LAY_DECADAS, 40, 30, 1, saida, "foco", 24, w);
  estado(VT_LAY_VITRINE, 12, 3, 0, saida, "vitrine", 0, w);

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  printf("%s\n", falhas ? "FAIL" : "PASS");
  return falhas ? 1 : 0;
}
