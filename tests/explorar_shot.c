// Captura a tela Explorar 2.0 SEM janela visivel: a janela GL nasce escondida,
// o desenho vai para um FBO e o quadro sai por glReadPixels, em PNG.
//
// So o caminho LOCAL (sem chave do TMDB, sem rede): e o que a tela tem de
// mostrar sozinha. Catalogo sintetico com cartazes do pacote
// (deploy/app/art/poster); nomes de pessoas sao de exemplo.
//
// Quadros:
//   1-climas     a grade de climas (portal)
//   2-clima      o primeiro clima aberto
//   3-vizinhanca a toca no primeiro titulo do clima
//   4-trilha     depois de descer dois degraus (trilha com tres passos)
//   5-subiu      Voltar uma vez: um degrau acima
//   6-detalhe    entrada pelo Detalhe (explorar_abrir_titulo)
// Alem das capturas, confere pelo retrato publicado (mapa_vizinhos_copiar)
// que cada OK desceu de fato, que Voltar subiu e que Voltar no primeiro degrau
// da entrada pelo Detalhe pede a pagina do titulo de volta.
#include "explorar.h"
#include "nvtmp.h"
#include "mapa.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "rail_shot.h"
#include "dados.h"
#include "vidro_telas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const char *saida;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  explorar_evento(&e);
}

static void quadros(int n, const char *nome) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(10);
    gfx_novo_quadro();
    vt_antes();
    explorar_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    explorar_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_EXPLORAR);
    vt_depois();
    if (vt_ligado()) continue;
    glFinish();
    if (nome && i == n - 1) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      char cam[700];
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      snprintf(cam, sizeof cam, "%s-%s.png", saida, nome);
      assert(IMG_SavePNG(s, cam) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("%s  desenhos=%d\n", cam, gfx_n_rect);
    }
    SDL_Delay(2);
  }
}

// --- catalogo sintetico ---------------------------------------------------------

typedef struct {
  const char *titulo, *genero, *meta, *poster, *tipo, *direcao, *ator;
  int nota, progresso;
} Linha;
static const Linha CAT[] = {
  { "The Prestige", "Filme  ·  Drama  ·  Mistério", "2006  ·  130 min", "02", "movie", "Lena Hart", "Tomás Weber", 85, 96 },
  { "Frequency", "Filme  ·  Crime  ·  Drama", "2000  ·  118 min", "05", "movie", "Iris Moon", "Paulo Vidal", 72, 96 },
  { "Prisoners", "Filme  ·  Crime  ·  Drama", "2013  ·  153 min", "09", "movie", "Iris Moon", "Tomás Weber", 81, 45 },
  { "The Martian", "Filme  ·  Aventura  ·  Drama", "2015  ·  141 min", "12", "movie", "Rui Sato", "Ana Kowalski", 80, 96 },
  { "3 Body Problem", "Série  ·  Ficção científica  ·  Mistério", "2024", "15", "series", "", "Ana Kowalski", 76, 45 },
  { "Lost", "Série  ·  Mistério  ·  Aventura", "2004", "21", "series", "", "Paulo Vidal", 83, 0 },
  { "The Mist", "Série  ·  Ficção científica  ·  Mistério", "2017", "19", "series", "", "Tomás Weber", 64, 0 },
  { "Maniac", "Série  ·  Comédia  ·  Drama", "2018", "39", "series", "", "Ana Kowalski", 77, 0 },
  { "Project Hail Mary", "Filme  ·  Aventura  ·  Ficção científica", "2026  ·  157 min", "13", "movie", "Rui Sato", "Paulo Vidal", 84, 0 },
  { "From", "Série  ·  Drama  ·  Terror", "2022", "04", "series", "", "Paulo Vidal", 77, 0 },
  { "Space/Time", "Filme  ·  Ficção científica  ·  Mistério", "2025  ·  90 min", "14", "movie", "Lena Hart", "Ana Kowalski", 61, 0 },
  { "Mr. K", "Filme  ·  Drama  ·  Mistério", "2025  ·  96 min", "38", "movie", "Lena Hart", "Tomás Weber", 66, 0 },
  { "Locke & Key", "Série  ·  Fantasia  ·  Drama", "2020", "27", "series", "", "Tomás Weber", 73, 0 },
  { "WandaVision", "Série  ·  Ficção científica  ·  Mistério", "2021", "31", "series", "", "Ana Kowalski", 79, 0 },
  { "Fallout", "Série  ·  Ação  ·  Aventura", "2024", "00", "series", "", "Paulo Vidal", 82, 0 },
  { "Extrapolations", "Série  ·  Drama  ·  Ficção científica", "2023", "17", "series", "", "Ana Kowalski", 60, 0 },
  { "The Umbrella Academy", "Série  ·  Ação  ·  Ficção científica", "2019", "35", "series", "", "Paulo Vidal", 76, 0 },
  { "IT: Welcome to Derry", "Série  ·  Drama  ·  Mistério", "2025", "29", "series", "", "Tomás Weber", 78, 0 },
  { "Zodiac", "Filme  ·  Crime  ·  Mistério", "2007  ·  157 min", "07", "movie", "Iris Moon", "Paulo Vidal", 77, 0 },
  { "Severance", "Série  ·  Drama  ·  Mistério", "2022", "23", "series", "", "Ana Kowalski", 87, 0 },
  { "Dark Comedy Night", "Filme  ·  Comédia  ·  Crime", "2019  ·  101 min", "33", "movie", "Rui Sato", "Paulo Vidal", 70, 0 },
};
#define NCAT (int)(sizeof CAT / sizeof CAT[0])

static void semearCatalogo(void) {
  static CatItem itens[NCAT];
  int i;
  memset(itens, 0, sizeof itens);
  for (i = 0; i < NCAT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt-exp-%02d", i);
    snprintf(c->tipo, sizeof c->tipo, "%s", CAT[i].tipo);
    snprintf(c->titulo, sizeof c->titulo, "%s", CAT[i].titulo);
    snprintf(c->genero, sizeof c->genero, "%s", CAT[i].genero);
    snprintf(c->meta, sizeof c->meta, "%s", CAT[i].meta);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%s.jpg", CAT[i].poster);
    snprintf(c->direcao, sizeof c->direcao, "%s", CAT[i].direcao);
    snprintf(c->elenco[0].nome, sizeof c->elenco[0].nome, "%s", CAT[i].ator);
    c->nElenco = 1;
    c->nota = CAT[i].nota;
    c->progresso = CAT[i].progresso;
  }
  cat_definir_tudo(itens, NCAT, NULL, 0);
}

static void focoPublicado(char *dst, size_t n, int *grupos) {
  static MapaVizinhos v;
  unsigned rev = 0;
  int g;
  assert(mapa_vizinhos_copiar(&v, &rev));
  snprintf(dst, n, "%s", v.foco.titulo);
  *grupos = 0;
  for (g = 0; g < MAPA_VIZ_GRUPOS; g++) if (v.g[g].n > 0) (*grupos)++;
}

int main(int argc, char **argv) {
  SDL_Window *win;
  SDL_GLContext gl;
  char raiz[128], passo1[128], passo2[128], volta[128];
  int grupos;
  saida = argc > 1 ? argv[1] : nvt("/nuvio-explorar");
  // Idioma, acento e animacoes pelo caminho de verdade (ajustes.txt na pasta
  // de dados temporaria): NUVIO_SHOT_EN=1 para ingles, NUVIO_SHOT_THEME=<n>
  // para o acento, NUVIO_SHOT_REDUZ=1 para animacoes reduzidas.
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  { char caminho[700]; FILE *f;
    const char *en = getenv("NUVIO_SHOT_EN"), *tema = getenv("NUVIO_SHOT_THEME");
    const char *reduz = getenv("NUVIO_SHOT_REDUZ");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
    f = fopen(caminho, "w");
    assert(f);
    fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n",
            en && *en == '1', tema && *tema ? atoi(tema) : 2, reduz && *reduz == '1');
    fclose(f);
    ajustes_dir(dados_dir()); }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  win = SDL_CreateWindow("explorar-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(win);
  gl = SDL_GL_CreateContext(win);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  semearCatalogo();

  // Os climas saem do catalogo, sem rede: o primeiro (maior afinidade) tem de
  // ter titulos.
  { static MapaClimas cl;
    unsigned rev = 0;
    mapa_climas_pedir();
    assert(mapa_climas_copiar(&cl, &rev));
    assert(cl.n == MAPA_CLIMA_N && cl.c[0].n > 0 && cl.c[0].afinidade >= cl.c[1].afinidade); }

  explorar_iniciar();
  // NUVIO_VIDRO_TELAS=<dir> (tests/vidro_telas.sh): glass budget, climates.
  if (vt_ligado()) {
    int i;
    vt_vidro();
    for (i = 0; i < 150; i++) { quadros(1, NULL); SDL_Delay(2); }
    vt_zerar(); quadros(30, NULL); vt_png("explorar"); vt_relatar("explore", 2);
    for (i = 0; i < 6; i++) { tecla(i & 1 ? SDLK_LEFT : SDLK_RIGHT); quadros(4, NULL); }
    vt_relatar("explore", 0);
    return vtFalhas ? 1 : 0;
  }
  quadros(90, "1-climas");
  tecla(SDLK_RETURN);
  quadros(70, "2-clima");

  tecla(SDLK_RETURN);
  quadros(70, "3-vizinhanca");
  focoPublicado(raiz, sizeof raiz, &grupos);
  assert(raiz[0] && grupos >= 2);

  // Dois degraus: o primeiro cartaz do primeiro grupo, depois o segundo cartaz.
  tecla(SDLK_RETURN);
  quadros(10, NULL);
  focoPublicado(passo1, sizeof passo1, &grupos);
  assert(strcmp(passo1, raiz) && grupos >= 1);
  tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  quadros(70, "4-trilha");
  focoPublicado(passo2, sizeof passo2, &grupos);
  assert(strcmp(passo2, passo1) && strcmp(passo2, raiz));

  tecla(SDLK_ESCAPE);
  quadros(50, "5-subiu");
  focoPublicado(volta, sizeof volta, &grupos);
  assert(!strcmp(volta, passo1));

  // Entrada pelo Detalhe: a toca comeca no titulo da pagina.
  explorar_iniciar();
  { MapaObra o;
    assert(mapa_obra_do_catalogo(2, &o));
    explorar_abrir_titulo(&o); }
  quadros(70, "6-detalhe");
  focoPublicado(volta, sizeof volta, &grupos);
  assert(!strcmp(volta, "Prisoners"));
  // Voltar no primeiro degrau devolve a pagina do titulo.
  tecla(SDLK_ESCAPE);
  { int idx = -1;
    assert(explorar_pediu_abrir(&idx) && idx == 2); }

  printf("explorar_shot: raiz=%s -> %s -> %s; capturas gravadas\n", raiz, passo1, passo2);
  explorar_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
  return 0;
}
