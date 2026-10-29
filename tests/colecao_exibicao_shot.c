// CAPTURA DAS EXIBICOES DA PAGINA DE COLECAO, sem rede (artes de deploy/app/art).
//
// Uma foto por exibicao (Grade, Linha do tempo, Ranking, Saga, Decadas, Mural),
// mais: o botao "Exibição" em foco, a Saga com capitulos vistos e as Decadas
// roladas (o cabecalho preso no topo). Fonte da TV com NUVIO_SHOT_FONTE=3.
//
//   bash tests/colecao_exibicao_shot.sh <pasta de saida>
#define desc_vertudo_n shot_vt_n
#define desc_vertudo_item shot_vt_item
#include "../src/vertudo.c"
#include "nvtmp.h"
#undef desc_vertudo_n
#undef desc_vertudo_item
#include "corviva.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static CatItem semente[122];
static int nSemente;
int shot_vt_n(void) { return nSemente; }
int shot_vt_item(int i, CatItem *dst) {
  if (i < 0 || i >= nSemente) return 0;
  if (dst) *dst = semente[i];
  return 1;
}

static const struct { const char *t; int ano; } FILMES[] = {
  { "A Ameaça Fantasma", 1999 }, { "Ataque dos Clones", 2002 },
  { "A Vingança dos Sith", 2005 }, { "Uma Nova Esperança", 1977 },
  { "O Império Contra-Ataca", 1980 }, { "O Retorno de Jedi", 1983 },
  { "O Despertar da Força", 2015 }, { "Os Últimos Jedi", 2017 },
  { "A Ascensão Skywalker", 2019 }, { "Rogue One", 2016 },
  { "Solo", 2018 }, { "Clone Wars", 2008 },
  { "O Farol", 1994 }, { "Mata Fechada", 1991 }, { "Noite de Verão", 2011 },
  { "Rio Acima", 1987 }, { "Cartas de Inverno", 1975 }, { "Sal e Luz", 2021 },
};
#define NF (int)(sizeof FILMES / sizeof *FILMES)

static SDL_Window *janela;
static void captura(const char *nome, int quadros) {
  int i;
  for (i = 0; i < quadros; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(8);
    gfx_novo_quadro();
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    vertudo_atualizar(1.0f / 60.0f, agora);
    vertudo_desenhar(agora);
    if (nome && i == quadros - 1) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      int y;
      assert(pix && s);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
      printf("captura: %s (fill=%.2f rects=%d)\n", nome, gfx_fill, gfx_n_rect);
    }
    SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  vertudo_evento(&e);
}

static ColFolder pasta;
static ColSource fontes[3];

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-colecao-exibicao");
  char nome[700], cam[700];
  SDL_GLContext gl;
  FILE *a;
  int i, L;

  dados_iniciar("deploy/app/art");
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dados_dir());
  a = fopen(cam, "w"); assert(a);
  fprintf(a, "idioma 0\n");
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(a);
  ajustes_dir(dados_dir());

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: exibicoes da colecao", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela); assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_borrao_iniciar(480, 270);
  gfx_icones_dir("deploy/app/art");

  nSemente = NF;
  for (i = 0; i < NF; i++) {
    CatItem *c = &semente[i];
    int k = (i * 7 + 3) % 40;
    snprintf(c->imdb, sizeof c->imdb, "tt80%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", FILMES[i].t);
    snprintf(c->genero, sizeof c->genero, "Ficção científica · Aventura");
    snprintf(c->meta, sizeof c->meta, "%d · 2 h 1%d min", FILMES[i].ano, i % 10);
    snprintf(c->sinopse, sizeof c->sinopse,
             "Uma sinopse de verdade tem duas ou tres linhas e ocupa o painel "
             "lateral sem empurrar a nota para fora da tela.");
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", k);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", k);
    if (k < 10) snprintf(c->logo, sizeof c->logo, "deploy/app/art/logo/%02d.png", k);
    c->nota = 70 + i % 20;
    // Ficha do painel: classificacao, direcao e elenco em parte dos titulos.
    snprintf(c->classificacao, sizeof c->classificacao, "%s", i % 3 ? "14" : "L");
    if (i % 2 == 1) {
      snprintf(c->direcao, sizeof c->direcao, "Marta Albuquerque");
      snprintf(c->elenco[0].nome, sizeof c->elenco[0].nome, "Ana Ribeiro");
      snprintf(c->elenco[1].nome, sizeof c->elenco[1].nome, "Caio Monteiro");
      snprintf(c->elenco[2].nome, sizeof c->elenco[2].nome, "Lia Fontes");
      c->nElenco = 3;
    }
    // Saga: os tres primeiros lancamentos vistos, o quarto pela metade.
    if (FILMES[i].ano < 1984) c->progresso = 100;
    if (FILMES[i].ano == 1999) c->progresso = 46;
  }

  pasta.sources = fontes;
  pasta.nSources = 2;
  snprintf(pasta.id, sizeof pasta.id, "starwars");
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  snprintf(fontes[0].title, sizeof fontes[0].title, "Filmes");
  snprintf(fontes[0].type, sizeof fontes[0].type, "movie");
  snprintf(fontes[1].title, sizeof fontes[1].title, "Séries");
  snprintf(fontes[1].type, sizeof fontes[1].type, "series");
  collection = &pasta;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  aberta = 1; anim = 1.0f; source = 0; tabFocus = 0;

  // Focused layout proof: identical 122-title fixture at both TV scales.
  if (getenv("NUVIO_SHOT_LAYOUT")) {
    const float scales[] = {0.85f, 1.0f};
    const int modes[] = {VT_LAY_MURAL, VT_LAY_FLUXO, VT_LAY_LINHA};
    const char *names[] = {"mural", "fluxo", "timeline"};
    for (i = NF; i < 122; i++) semente[i] = semente[i % NF];
    nSemente = 122;
    snprintf(pasta.title, sizeof pasta.title, "MARVEL");
    snprintf(titulo, sizeof titulo, "%s", pasta.title);
    snprintf(semente[1].titulo, sizeof semente[1].titulo,
             "Captain America: The Winter Soldier and the Return of the Avengers");
    for (int s = 0; s < 2; s++) {
      gfx_escala_txt_definir(scales[s]);
      for (int m = 0; m < 3; m++) {
        lay = layEscolha = modes[m];
        resolverLayout(); celLay = -1; orderN = -1;
        foco = 0; tabFocus = 0; scrollY = velY = 0;
        captura(NULL, 20);
        if (lay == VT_LAY_FLUXO)
          for (int k = 0; k < 6; k++) tecla(SDLK_RIGHT);
        snprintf(nome, sizeof nome, "%s-%s-%.2f.bmp", saida, names[m], scales[s]);
        captura(nome, 140);
      }
    }
    goto done;
  }

  captura(NULL, 60);   // artes chegando ao cache
  // PADRAO (#362): pasta com abas, exibicao Automatico -> Grade com a barra de
  // abas, NUNCA a Vitrine.
  layEscolha = VT_LAY_AUTO; lay = VT_LAY_MURAL; celLay = -1; orderN = -1; foco = 1;
  resolverLayout();
  assert(lay == VT_LAY_GRADE);
  snprintf(nome, sizeof nome, "%s-1-grade-padrao-abas.bmp", saida);
  captura(nome, 110);
  for (L = VT_LAY_GRADE; L < VT_LAY_VITRINE; L++) {
    lay = L; layEscolha = L; celLay = -1; orderN = -1; foco = 1; scrollY = velY = 0;
    armarOnda();
    snprintf(nome, sizeof nome, "%s-%d-%s.bmp", saida, L,
             (const char *[]){ "", "grade", "linha", "ranking", "saga", "decadas", "mural" }[L]);
    captura(nome, 110);
  }
  // Decadas rolada: o cabecalho da secao de cima fica preso.
  lay = VT_LAY_DECADAS; celLay = -1; orderN = -1; foco = 0;
  for (i = 0; i < 4; i++) tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-5-decadas-rolada.bmp", saida);
  captura(nome, 90);
  // Saga rolada ate a segunda linha e o botao em foco (com a dica).
  lay = VT_LAY_SAGA; celLay = -1; orderN = -1; foco = 0;
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-4-saga-linha2.bmp", saida);
  captura(nome, 80);
  tabFocus = 1; tabCursor = nAbas();
  snprintf(nome, sizeof nome, "%s-0-botao-exibicao.bmp", saida);
  captura(nome, 60);
  // Diretores: o retrato no canto e o botao estreito ao lado dele.
  snprintf(pasta.group, sizeof pasta.group, "Directors");
  snprintf(pasta.title, sizeof pasta.title, "Christopher Nolan");
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  pasta.nSources = 1; tabFocus = 0;
  lay = VT_LAY_LINHA; celLay = -1; orderN = -1; foco = 2;
  snprintf(nome, sizeof nome, "%s-2-linha-diretor.bmp", saida);
  captura(nome, 80);

  // VITRINE: tres abas viram tres fileiras (semeadas direto; a fila de abas
  // precisa de rede). Topo, e rolada ate a terceira fileira.
  pasta.nSources = 3; tabFocus = 0;
  snprintf(pasta.group, sizeof pasta.group, "Film Collections");
  snprintf(pasta.title, sizeof pasta.title, "Star Wars");
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  snprintf(fontes[2].title, sizeof fontes[2].title, "Animações");
  snprintf(fontes[2].type, sizeof fontes[2].type, "movie");
  lay = VT_LAY_VITRINE; layEscolha = VT_LAY_VITRINE; celLay = -1;
  vitIniciar();
  { static const char *NOMES[3] = { "Filmes", "Séries", "Animações" };
    for (i = 0; i < 3; i++) {
      int k;
      snprintf(vitLin[i].nome, sizeof vitLin[i].nome, "%s", NOMES[i]);
      vitLin[i].estado = VIT_PRONTA; vitLin[i].n = 12;
      for (k = 0; k < 12; k++) vitBuf[i * vitCap + k] = semente[(i * 5 + k) % NF];
    }
    vitCarga = vitN; vitMexeu = 1; vitFR = 0; vitFC = 2; }
  captura(NULL, 90);
  snprintf(nome, sizeof nome, "%s-7-vitrine-topo.bmp", saida);
  captura(nome, 90);
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-7-vitrine-fileira3.bmp", saida);
  captura(nome, 140);
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_UP);
  snprintf(nome, sizeof nome, "%s-7-vitrine-botao.bmp", saida);
  captura(nome, 90);

  // STREAMING (13/10): wordmark no lugar do titulo, sem o rotulo, degrade na
  // cor da marca. O wordmark vem de um logo local (o do TMDB precisa de rede).
  snprintf(pasta.group, sizeof pasta.group, "Streaming");
  snprintf(pasta.title, sizeof pasta.title, "Netflix");
  snprintf(pasta.logo, sizeof pasta.logo, "deploy/app/art/logo/00.png");
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  captura(NULL, 60);
  snprintf(nome, sizeof nome, "%s-8-streaming-netflix.bmp", saida);
  captura(nome, 90);
  snprintf(pasta.title, sizeof pasta.title, "Canal Desconhecido");
  pasta.logo[0] = 0;
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  captura(NULL, 30);
  snprintf(nome, sizeof nome, "%s-8-streaming-desconhecido.bmp", saida);
  captura(nome, 60);
  snprintf(pasta.title, sizeof pasta.title, "Netflix");
  snprintf(pasta.logo, sizeof pasta.logo, "deploy/app/art/logo/00.png");
  snprintf(titulo, sizeof titulo, "%s", pasta.title);
  pasta.nSources = 1; lay = VT_LAY_GRADE; layEscolha = VT_LAY_GRADE; celLay = -1; orderN = -1; foco = 1;
  snprintf(nome, sizeof nome, "%s-8-streaming-grade.bmp", saida);
  captura(nome, 90);

  // STREAMING com seis abas (13/10): as abas esmaecem antes do botao, sem a
  // legenda de depuracao, e cada exibicao com a ficha nova no painel.
  { static ColSource seis[6];
    static const char *ABAS[6] = { "Filmes", "Séries", "Top 10 · Filmes", "Top 10 · Séries",
                                   "Kids Top 10 · Filmes", "Kids Top 10 · Séries" };
    for (i = 0; i < 6; i++) {
      snprintf(seis[i].title, sizeof seis[i].title, "%s", ABAS[i]);
      snprintf(seis[i].type, sizeof seis[i].type, "%s", i & 1 ? "series" : "movie");
      snprintf(seis[i].base, sizeof seis[i].base, "http://shot.local");
    }
    pasta.sources = seis; pasta.nSources = 6; tabFocus = 0; tabRol = 0;
    for (L = VT_LAY_GRADE; L < VT_LAY_VITRINE; L++) {
      lay = L; layEscolha = L; celLay = -1; orderN = -1; foco = 1; scrollY = velY = 0;
      resolverLayout(); armarOnda();
      snprintf(nome, sizeof nome, "%s-9-streaming-%s.bmp", saida,
               (const char *[]){ "", "grade", "linha", "ranking", "saga", "decadas", "mural" }[L]);
      captura(nome, 110);
    }
    // Linha do tempo com o foco no quarto ano: a linha preenchida ate ele.
    lay = VT_LAY_LINHA; layEscolha = VT_LAY_LINHA; resolverLayout(); celLay = -1; orderN = -1;
    foco = 0; captura(NULL, 20);
    for (i = 0; i < 3; i++) { tecla(SDLK_DOWN); captura(NULL, 8); }
    snprintf(nome, sizeof nome, "%s-9-streaming-linha-foco4.bmp", saida);
    captura(nome, 90);
    // A horizontal (Cover Flow), com o foco no meio da cronologia.
    lay = VT_LAY_FLUXO; layEscolha = VT_LAY_FLUXO; resolverLayout(); celLay = -1; orderN = -1;
    foco = 0; captura(NULL, 20);
    for (i = 0; i < 6; i++) { tecla(SDLK_RIGHT); captura(NULL, 6); }
    snprintf(nome, sizeof nome, "%s-9-streaming-fluxo.bmp", saida);
    captura(nome, 90);
    lay = VT_LAY_GRADE; layEscolha = VT_LAY_GRADE; resolverLayout(); celLay = -1;
    tabFocus = 1; tabCursor = nAbas();
    snprintf(nome, sizeof nome, "%s-9-streaming-botao.bmp", saida);
    captura(nome, 60);
    tabFocus = 0;
    // A Vitrine segue o layout da Home: uma foto em cada um.
    lay = VT_LAY_VITRINE; layEscolha = VT_LAY_VITRINE; celLay = -1;
    vitIniciar();
    for (i = 0; i < vitN; i++) {
      int k;
      vitLin[i].estado = VIT_PRONTA; vitLin[i].n = 12;
      for (k = 0; k < 12; k++) vitBuf[i * vitCap + k] = semente[(i * 5 + k) % NF];
    }
    vitCarga = vitN; vitMexeu = 1; vitFR = 0; vitFC = 2;
    for (L = HOME_LAYOUT_MODERNA; L < HOME_LAYOUT_N; L++) {
      a = fopen(cam, "w"); assert(a);
      fprintf(a, "idioma 0\nhomeLayoutLocal %d\n", L);
      if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
      fclose(a);
      ajustes_dir(dados_dir());
      snprintf(nome, sizeof nome, "%s-9-streaming-vitrine-%s.bmp", saida,
               (const char *[]){ "moderna", "padrao", "dinamica" }[L]);
      captura(nome, 120);
    } }

done:
  tex_encerrar();
  txt_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
