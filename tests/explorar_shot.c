// Captura a tela Explorar (o ceu das historias) SEM janela visivel: a janela
// GL nasce escondida, o desenho vai para um FBO e o quadro sai por
// glReadPixels. Dois momentos:
//   1. "local": so o catalogo sintetico, como abre sem TMDB;
//   2. "cruzado": um cruzamento pronto publicado por mapa_publicar_teste, com
//      titulos e cartazes do pacote (deploy/app/art) — os ELOS sao dados de
//      exemplo escritos aqui, nao resposta do TMDB.
// Tambem imprime o custo de cada quadro capturado (desenhos, trocas de
// programa, area preenchida em telas cheias, ms de CPU no desenho).
#include "explorar.h"
#include "mapa.h"
#include "catalogo.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "ajustes.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;
static const char *saida = "/tmp/nuvio-explorar";

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  explorar_evento(&e);
}

static void quadros(int n, const char *nome) {
  int i;
  for (i = 0; i < n; i++) {
    Uint64 t0;
    double ms;
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(10);
    gfx_novo_quadro();
    explorar_atualizar(1.0f / 60.0f, SDL_GetTicks());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    t0 = SDL_GetPerformanceCounter();
    explorar_desenhar(SDL_GetTicks());
    glFinish();
    ms = (double)(SDL_GetPerformanceCounter() - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
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
      snprintf(cam, sizeof cam, "%s-%s.bmp", saida, nome);
      assert(SDL_SaveBMP(s, cam) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("%-14s desenhos=%3d programas=%2d preenchido=%.2f telas cheias=%d cpu=%.2f ms\n",
             nome, gfx_n_rect, gfx_n_prog, gfx_fill, gfx_n_cheio, ms);
    }
    SDL_Delay(2);
  }
}

// --- catalogo sintetico: titulos e cartazes do pacote ---------------------------

typedef struct { const char *titulo, *genero, *meta, *poster, *tipo; int nota; } Linha;
static const Linha CAT[] = {
  { "The Prestige", "Filme  ·  Drama  ·  Mistério", "2006  ·  130 min", "02", "movie", 85 },
  { "Frequency", "Filme  ·  Crime  ·  Drama", "2000  ·  118 min", "05", "movie", 72 },
  { "Prisoners", "Filme  ·  Crime  ·  Drama", "2013  ·  153 min", "09", "movie", 81 },
  { "The Martian", "Filme  ·  Aventura  ·  Drama", "2015  ·  141 min", "12", "movie", 80 },
  { "3 Body Problem", "Série  ·  Ficção científica  ·  Mistério", "2024", "15", "series", 76 },
  { "Lost", "Série  ·  Mistério  ·  Aventura", "2004", "21", "series", 83 },
  { "The Mist", "Série  ·  Ficção científica  ·  Mistério", "2017", "19", "series", 64 },
  { "Maniac", "Série  ·  Comédia  ·  Drama", "2018", "39", "series", 77 },
  { "Project Hail Mary", "Filme  ·  Aventura  ·  Comédia", "2026  ·  157 min", "13", "movie", 84 },
  { "From", "Série  ·  Drama  ·  Terror", "2022", "04", "series", 77 },
  { "Space/Time", "Filme  ·  Ficção científica  ·  Ação", "2025  ·  90 min", "14", "movie", 61 },
  { "Mr. K", "Filme  ·  Drama  ·  Mistério", "2025  ·  96 min", "38", "movie", 66 },
  { "Locke & Key", "Série  ·  Ficção científica  ·  Drama", "2020", "27", "series", 73 },
  { "WandaVision", "Série  ·  Ficção científica  ·  Mistério", "2021", "31", "series", 79 },
  { "Fallout", "Série  ·  Ação  ·  Aventura", "2024", "00", "series", 82 },
  { "Extrapolations", "Série  ·  Drama  ·  Ficção científica", "2023", "17", "series", 60 },
  { "The Umbrella Academy", "Série  ·  Ação  ·  Ficção científica", "2019", "35", "series", 76 },
  { "IT: Welcome to Derry", "Série  ·  Drama  ·  Mistério", "2025", "29", "series", 78 },
};
#define NCAT (int)(sizeof CAT / sizeof CAT[0])

static void poster(char *dst, size_t n, const char *id) {
  snprintf(dst, n, "deploy/app/art/poster/%s.jpg", id);
}

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
    poster(c->poster, sizeof c->poster, CAT[i].poster);
    snprintf(c->sinopse, sizeof c->sinopse, "%s",
             "Uma historia de exemplo para medir o corte da sinopse no painel lateral, "
             "com o comprimento de uma sinopse real do TMDB.");
    c->nota = CAT[i].nota;
    // As oito primeiras sao o "historico": progresso visto ou em andamento.
    if (i < 8) c->progresso = (i % 3 == 0) ? 45 : 96;
  }
  cat_definir_tudo(itens, NCAT, NULL, 0);
}

// --- cruzamento de exemplo ------------------------------------------------------

static MapaObra obra(int i, long tmdb) {
  MapaObra o;
  memset(&o, 0, sizeof o);
  o.tmdb = tmdb;
  o.catIndice = i;
  snprintf(o.imdb, sizeof o.imdb, "tt-exp-%02d", i);
  snprintf(o.tipo, sizeof o.tipo, "%s", CAT[i].tipo);
  snprintf(o.titulo, sizeof o.titulo, "%s", CAT[i].titulo);
  poster(o.poster, sizeof o.poster, CAT[i].poster);
  snprintf(o.sinopse, sizeof o.sinopse, "%s",
           "Sinopse de exemplo com o tamanho de uma real, para conferir quantas linhas "
           "cabem no painel e onde o texto corta sem invadir a acao.");
  o.ano = atoi(CAT[i].meta);
  o.nota = CAT[i].nota;
  o.votos = 5000 + i * 700;
  return o;
}

static void kw(MapaSemente *s, long id, const char *nome) {
  s->kw[s->nKw].id = id;
  snprintf(s->kw[s->nKw].nome, sizeof s->kw[0].nome, "%s", nome);
  s->nKw++;
}
static void gen(MapaSemente *s, long id, const char *nome) {
  s->gen[s->nGen].id = id;
  snprintf(s->gen[s->nGen].nome, sizeof s->gen[0].nome, "%s", nome);
  s->nGen++;
}
static void gente(MapaSemente *s, long id, const char *nome, int dir) {
  s->gente[s->nGente].id = id;
  snprintf(s->gente[s->nGente].nome, sizeof s->gente[0].nome, "%s", nome);
  s->gente[s->nGente].direcao = dir;
  s->nGente++;
}
static void rec(MapaSemente *s, int i) {
  s->rec[s->nRec].o = obra(i, 9000 + i);
  s->rec[s->nRec].generos[0] = 18;
  s->rec[s->nRec].generos[1] = 9648;
  s->rec[s->nRec].nGen = 2;
  s->nRec++;
}

static void publicarCruzado(void) {
  static MapaSemente s[8];
  static MapaCreditos c[1];
  int i;
  memset(s, 0, sizeof s);
  for (i = 0; i < 8; i++) {
    s[i].obra = obra(i, 100 + i);
    s[i].quando = 1;
    s[i].origem = (i % 3 == 0) ? MAPA_ORIGEM_ANDAMENTO : MAPA_ORIGEM_VISTO;
    gen(&s[i], 18, "Drama");
  }
  gen(&s[0], 9648, "Mistério"); kw(&s[0], 11800, "twist ending"); kw(&s[0], 1, "memory");
  gente(&s[0], 6968, "Hugh Jackman", 0); gente(&s[0], 525, "Christopher Nolan", 1);
  kw(&s[1], 4379, "time travel"); gen(&s[1], 80, "Crime");
  gen(&s[2], 80, "Crime"); kw(&s[2], 11800, "twist ending"); kw(&s[2], 6149, "small town");
  gente(&s[2], 6968, "Hugh Jackman", 0); gente(&s[2], 137427, "Denis Villeneuve", 1);
  kw(&s[3], 9882, "space"); gen(&s[3], 12, "Aventura");
  kw(&s[4], 9882, "space"); kw(&s[4], 4379, "time travel"); gen(&s[4], 878, "Ficção científica");
  kw(&s[5], 4379, "time travel"); gen(&s[5], 9648, "Mistério");
  kw(&s[6], 6149, "small town"); gen(&s[6], 878, "Ficção científica");
  kw(&s[7], 1, "memory");
  rec(&s[3], 8); rec(&s[4], 8);            // Project Hail Mary: das duas do espaco
  rec(&s[6], 9); rec(&s[2], 9);            // From: cidade pequena
  rec(&s[1], 10); rec(&s[5], 10);          // Space/Time: viagem no tempo
  rec(&s[0], 11); rec(&s[2], 11);          // Mr. K
  rec(&s[7], 12); rec(&s[0], 12);          // Locke & Key: memoria
  rec(&s[4], 13); rec(&s[6], 14); rec(&s[5], 15); rec(&s[1], 16); rec(&s[3], 17);
  memset(c, 0, sizeof c);
  c[0].pessoa = 6968;
  c[0].n = 1;
  c[0].obras[0] = obra(17, 263115);
  snprintf(c[0].obras[0].titulo, sizeof c[0].obras[0].titulo, "%s", "Logan");
  c[0].obras[0].poster[0] = 0;            // sem cartaz no pacote: o painel mostra o vazio
  c[0].obras[0].ano = 2017;
  c[0].obras[0].nota = 78;
  mapa_publicar_teste(s, 8, c, 1);
}

int main(int argc, char **argv) {
  SDL_Window *win;
  SDL_GLContext gl;
  if (argc > 1) saida = argv[1];
  // Idioma, acento e animacoes pelo caminho de verdade (ajustes.txt na pasta
  // de dados temporaria), como em social_shot: NUVIO_SHOT_EN=1 para ingles,
  // NUVIO_SHOT_THEME=<n> para o acento, NUVIO_SHOT_REDUZ=1 para animacoes
  // reduzidas.
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
  // Nada na tela do Mac: sem icone no Dock e janela escondida.
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

  explorar_iniciar();
  quadros(40, "entrada");
  quadros(140, "local");

  publicarCruzado();
  quadros(150, "cruzado");
  tecla(SDLK_LEFT);  quadros(50, "foco-esq");
  tecla(SDLK_UP);    quadros(50, "foco-cima");
  tecla(SDLK_UP);    quadros(50, "foco-cima2");
  tecla(SDLK_RIGHT); quadros(50, "foco-dir");
  tecla(SDLK_RIGHT); quadros(50, "foco-dir2");
  tecla(SDLK_DOWN);  quadros(50, "foco-baixo");
  // O dado mora no canto de baixo a direita: direita ate o fim, depois desce.
  { int i; for (i = 0; i < 6; i++) tecla(SDLK_RIGHT); tecla(SDLK_DOWN); tecla(SDLK_DOWN); }
  quadros(50, "dado");
  tecla(SDLK_RETURN); quadros(40, "giro");
  quadros(120, "sorteado");

  puts("explorar_shot: capturas gravadas");
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
