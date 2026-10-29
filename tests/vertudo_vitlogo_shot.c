// VITRINE DA COLECAO (job 205-vitlogo1015), em PNG + geometria.
//
//   largo.png      logo largo (4,6:1, como o de DUNE com a folga transparente).
//   alto.png       logo alto (1,2:1).
//   semlogo.png    sem logo: o nome em texto.
//   recolhido.png  foco no seletor: so a exibicao aberta.
//   aberto.png     depois do OK: o trilho inteiro, cursor um a esquerda.
//
// O topo do bloco do destaque sai da DIFERENCA entre dois quadros: o titulo com
// o texto e o mesmo titulo sem texto (a arte e o cabecalho ficam iguais). Ele
// precisa ficar abaixo do pe do cabecalho (anel + legenda) com folga. Sai 1 se
// algo falha. Nao entra na suite: precisa de janela GL.
static int shotN, shotVistos;
static const char *shotLogo = "";
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
  if (i < 0 || i >= shotN || !dst) return 0;
  memset(dst, 0, sizeof *dst);
  snprintf(dst->imdb, sizeof dst->imdb, "tt%07d", 100 + i);
  snprintf(dst->tipo, sizeof dst->tipo, "movie");
  snprintf(dst->titulo, sizeof dst->titulo, "%s", "Blade Runner 2049");
  snprintf(dst->genero, sizeof dst->genero, "Movie · Science Fiction");
  snprintf(dst->meta, sizeof dst->meta, "%d · 155 min", 2023 - i);
  dst->nota = 80;   // a linha do IMDb, como na TCL
  snprintf(dst->sinopse, sizeof dst->sinopse, "%s",
           "Paul Atreides, a brilliant and gifted young man born into a great destiny beyond his "
           "understanding, must travel to the most dangerous planet in the universe to ensure the "
           "future of his family and his people.");
  snprintf(dst->logo, sizeof dst->logo, "%s", shotLogo);
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
static unsigned char pixA[1920 * 1080 * 4], pixB[1920 * 1080 * 4];

static void quadros(int n, unsigned char *pix, const char *png, SDL_Window *win) {
  for (int i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    vertudo_atualizar(1.0f / 60.0f, SDL_GetTicks());
    vertudo_desenhar(SDL_GetTicks());
    if (i == n - 1) glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
    SDL_GL_SwapWindow(win);
  }
  if (png) {
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
    assert(s);
    for (int y = 0; y < 1080; y++)
      memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
    assert(IMG_SavePNG(s, png) == 0);
    SDL_FreeSurface(s);
    printf("captura: %s\n", png);
  }
}

static void confere(int ok, const char *estado, const char *o_que) {
  printf("%s %s: %s\n", ok ? "PASS" : "FAIL", estado, o_que);
  falhas += !ok;
}

// Primeira linha (de cima) em que os dois quadros diferem na coluna do texto,
// acima das fileiras. glReadPixels e de baixo para cima.
static float topoDiferenca(float x0, float ate) {
  for (int y = 0; y < (int)ate; y++) {
    const unsigned char *a = pixA + (1079 - y) * 1920 * 4, *b = pixB + (1079 - y) * 1920 * 4;
    for (int x = (int)x0; x < (int)x0 + 900; x++)
      for (int c = 0; c < 3; c++)
        if (abs(a[x * 4 + c] - b[x * 4 + c]) > 24) return (float)y;
  }
  return ate;
}

static void destaque(const char *logo, const char *saida, const char *nome, SDL_Window *w) {
  char cam[600];
  CatItem *h, guarda;
  float pe = NV_ILHA_Y + NV_ILHA_H + 14.0f + NV_CTRL_H * 0.5f + 34.0f;   // anel R+4, legenda
  float topo;
  shotLogo = logo;
  layEscolha = VT_LAY_VITRINE; resolverLayout(); vitIniciar(); vistosEm = 0;
  tabFocus = 0; vitMexeu = 0;
  vcTotal = shotN; vcVistos = 0;   // a Vitrine nao conta: vem da grade aberta antes
  quadros(90, pixA, NULL, w);
  h = vitChave(vitHeroAtual);
  assert(h);
  guarda = *h;
  h->titulo[0] = h->meta[0] = h->genero[0] = h->sinopse[0] = h->logo[0] = 0;
  quadros(20, pixB, NULL, w);
  *h = guarda;
  snprintf(cam, sizeof cam, "%s/%s.png", saida, nome);
  quadros(20, pixA, cam, w);
  topo = topoDiferenca(vtInicio(), vitTopo() - 8.0f);
  printf("%s: hero top %.0f, header bottom %.0f\n", nome, topo, pe);
  confere(topo >= pe + 16.0f, nome, "hero block starts below the header with a gap");
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-vitlogo");
  char cam[600];
  SDL_Window *w;
  SDL_GLContext gl;
  pasta.sources = pastaFontes;
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: vitrine", SDL_WINDOWPOS_CENTERED,
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

  snprintf(pasta.title, sizeof pasta.title, "Denis Villeneuve");
  snprintf(pasta.group, sizeof pasta.group, "Directors");
  snprintf(pastaFontes[0].title, sizeof pastaFontes[0].title, "Denis Villeneuve");
  snprintf(pastaFontes[0].type, sizeof pastaFontes[0].type, "movie");
  pasta.nSources = 1;
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0;
  shotN = 21; shotVistos = 0;

  destaque("deploy/app/art/logo/28.png", saida, "largo", w);
  destaque("deploy/app/art/logo/26.png", saida, "alto", w);
  destaque("", saida, "semlogo", w);

  // O seletor: recolhido com o foco nele; OK abre o trilho, a seta anda nele e
  // Voltar recolhe sem fechar a tela.
  tabFocus = 1; tabCursor = cabN();
  snprintf(cam, sizeof cam, "%s/recolhido.png", saida);
  quadros(30, pixA, cam, w);
#ifdef VIT_CAB_FOLGA   // o codigo antigo trocava (e gravava) a exibicao no OK
  {
    float recolhido = cabSw.w;
    SDL_Event e = { 0 };
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_RETURN; vertudo_evento(&e);
    e.key.keysym.sym = SDLK_LEFT; vertudo_evento(&e);
    snprintf(cam, sizeof cam, "%s/aberto.png", saida);
    quadros(30, pixA, cam, w);
    printf("switcher: collapsed %.0f px, expanded %.0f px\n", recolhido, cabSw.w);
    confere(recolhido < 320.0f && cabSw.w > recolhido + 300.0f && lay == VT_LAY_VITRINE,
            "seletor", "collapsed to one button, OK expands without switching");
    e.key.keysym.sym = SDLK_ESCAPE; vertudo_evento(&e);
    quadros(2, pixA, NULL, w);
    confere(aberta && cabSw.w == recolhido, "seletor", "Back collapses and keeps the screen");
  }
#endif

  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  printf("%s\n", falhas ? "FAIL" : "PASS");
  return falhas ? 1 : 0;
}
