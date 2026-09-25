// CAPTURA DA TELA "TROCAR ARTE" (#142) sobre a pagina do titulo, sem rede.
//
// O que ela responde, e que teste de logica nao responde:
//   o circular novo aparece no fim da linha de acoes e o OK nele abre a tela?
//   a grade de fundos 16:9 e a de logos leem como escolha, com o Automatico
//     primeiro e a marca na escolha vigente?
//   andar pelas miniaturas TROCA O FUNDO DA PAGINA (a previa) — e a Cor viva
//     vai junto, porque ela le a mesma arte?
//   o OK grava e a pagina, fechada a tela, desenha a escolha (a precedencia de
//     artehero ponta a ponta), e reabrir marca a escolha certa?
//
// SEM REDE: o titulo nao tem id do IMDb ("ensaio:1"), entao metahub, Apple
// TV e fanart.tv nao entram, e o fio do TMDB nem sai. As miniaturas "do
// TMDB" sao arquivos do pacote injetados por trocaarte_teste_candidato; as
// abas, o foco e a previa sao os de verdade. extras.h e interceptado como em
// tests/detail_secoes_shot.c para a pagina nao pedir nada.
//
//   bash tests/trocaarte_shot.sh /tmp/nuvio-trocaarte
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define extras_pedir            fx_pedir
#define extras_carregando       fx_zero
#define extras_n_temporadas     fx_zero
#define extras_n_comentarios    fx_zero
#define extras_n_comentarios_ep fx_zero
#define extras_n_relacionados   fx_zero
#define extras_n_colecao        fx_zero
#define extras_n_estudios       fx_zero
#define extras_n_trailers       fx_zero
#define extras_nota_trakt       fx_zero
#include "../src/detail.c"
#include "dados.h"
#include "arteescolha.h"

void fx_pedir(const char *imdb, int serie, long tmdbId) { (void)imdb; (void)serie; (void)tmdbId; }
int fx_zero(void) { return 0; }

static CatItem itens[1];
static SDL_Window *janela;

static void montar(void) {
  CatFileira fil;
  int i;
  memset(itens, 0, sizeof itens);
  memset(&fil, 0, sizeof fil);
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Filme de Ensaio");
  snprintf(itens[0].imdb, sizeof itens[0].imdb, "ensaio:1");
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "movie");
  snprintf(itens[0].genero, sizeof itens[0].genero, "Filme · Ficção científica");
  snprintf(itens[0].meta, sizeof itens[0].meta, "1999 · 2 h 16 min");
  snprintf(itens[0].sinopse, sizeof itens[0].sinopse,
           "Sinopse de enchimento, comprida o bastante para o bloco de texto do "
           "heroi ficar com a altura que tem num titulo de verdade.");
  snprintf(itens[0].backdrop, sizeof itens[0].backdrop, "deploy/app/art/07.jpg");
  snprintf(itens[0].logo, sizeof itens[0].logo, "deploy/app/art/logo/07.png");
  itens[0].nota = 87;
  for (i = 0; i < 6; i++) {
    snprintf(itens[0].elenco[i].nome, sizeof itens[0].elenco[i].nome, "Elenco %d", i + 1);
    snprintf(itens[0].elenco[i].papel, sizeof itens[0].elenco[i].papel, "Papel %d", i + 1);
  }
  itens[0].nElenco = 6;
  fil.ini = 0; fil.n = 1;
  snprintf(fil.titulo, sizeof fil.titulo, "Ensaio");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  cat_definir_tudo(itens, 1, &fil, 1);
}

static void gravar(const char *nome) {
  unsigned char *pix = (unsigned char *)malloc(1920 * 1080 * 4);
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
  printf("captura: %s  (%d desenhos gfx no quadro)\n", nome, gfx_n_rect);
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    SDL_PumpEvents();
    tex_bombear(4);
    detail_atualizar(1.0f / 60.0f, SDL_GetTicks());
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    detail_desenhar(SDL_GetTicks());
    if (i < n - 1) SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  detail_evento(&e);
  e.type = SDL_KEYUP;
  detail_evento(&e);
  quadros(2);
}

static void abrirPagina(void) {
  HomeItem hi;
  memset(&hi, 0, sizeof hi);
  hi.indice = 0;
  hi.rect.w = NV_TELA_W; hi.rect.h = NV_TELA_H;
  hi.titulo = itens[0].titulo; hi.genero = itens[0].genero; hi.meta = itens[0].meta;
  detail_abrir(&hi);
  quadros(120);
}

// O OK no ULTIMO circular da linha, pelo caminho de verdade (detail_evento).
static void abrirTroca(void) {
  nivel = 0;
  botao = nBotoes() - 1;
  assert(acaoEm(botao) == ACAO_ARTE);
  quadros(10);
  tecla(SDLK_RETURN);
  assert(trocaarte_aberto());
}

static void injetar(void) {
  static const char *F[] = { "03", "12", "21", "30", "35", "16", "25", "33" };
  static const char *L[] = { "03", "12", "21", "05" };
  char u[128];
  int i;
  for (i = 0; i < 8; i++) {
    snprintf(u, sizeof u, "deploy/app/art/%s.jpg", F[i]);
    trocaarte_teste_candidato(0, u, i % 3 == 2 ? "TMDB · EN" : "TMDB");
  }
  for (i = 0; i < 4; i++) {
    snprintf(u, sizeof u, "deploy/app/art/logo/%s.png", L[i]);
    trocaarte_teste_candidato(1, u, i == 1 ? "TMDB · EN" : "TMDB");
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-trocaarte";
  char nome[600], abs[PATH_MAX];
  SDL_GLContext gl;
  const char *dd;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  janela = SDL_CreateWindow("Nuvio: trocar arte", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                            1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  assert(realpath("deploy/app/art", abs));
  gfx_icones_dir(abs);
  tex_iniciar(64);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  dd = dados_dir();
  if (!dd || !strstr(dd, "nuvio-trocaarte-dados")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de apontar para a pasta temporaria "
                    "do teste (dados_dir = \"%s\")\n", dd ? dd : "");
    return 1;
  }
  // O que o main registra.
  artehero_definir_falhou(tex_falhou);
  artehero_definir_escolha(arteesc_fundo, arteesc_logo);
  arteesc_definir_perfil(1);

  montar();
  abrirPagina();
  snprintf(nome, sizeof nome, "%s-0-pagina.png", saida);
  gravar(nome);

  abrirTroca();
  injetar();
  assert(trocaarte_n(0) == 9 && trocaarte_n(1) == 5);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-1-fundos-automatico.png", saida);
  gravar(nome);

  // PREVIA: tres para a direita, e o fundo da pagina e o da miniatura.
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-2-fundos-previa.png", saida);
  gravar(nome);
  { const char *pv = trocaarte_previa_fundo();
    assert(pv && !strcmp(pv, "deploy/app/art/21.jpg"));
    assert(!strcmp(arteDe(idx), "deploy/app/art/21.jpg")); }

  // SEGUNDA LINHA.
  tecla(SDLK_DOWN);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-3-fundos-linha2.png", saida);
  gravar(nome);

  // ABA LOGOS: cima ate as abas, direita, baixo, e um logo para a direita.
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_RIGHT); tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  quadros(90);
  snprintf(nome, sizeof nome, "%s-4-logos-previa.png", saida);
  gravar(nome);
  assert(!trocaarte_previa_fundo());      // na aba de logos o fundo e o vigente

  // OK NO LOGO: grava e fecha.
  tecla(SDLK_RETURN);
  assert(!trocaarte_aberto());
  assert(arteesc_logo("ensaio:1") && !strcmp(arteesc_logo("ensaio:1"), "deploy/app/art/logo/12.png"));

  // FUNDO: reabre, escolhe o quarto e confere a pagina depois do OK.
  abrirTroca();
  injetar();
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  quadros(60);
  tecla(SDLK_RETURN);
  assert(!trocaarte_aberto());
  assert(arteesc_fundo("ensaio:1") && !strcmp(arteesc_fundo("ensaio:1"), "deploy/app/art/30.jpg"));
  quadros(90);
  assert(!strcmp(arteDe(idx), "deploy/app/art/30.jpg"));
  snprintf(nome, sizeof nome, "%s-5-pagina-escolhida.png", saida);
  gravar(nome);

  // REABRIR: o foco comeca na escolha vigente, com a marca.
  abrirTroca();
  injetar();
  quadros(90);
  snprintf(nome, sizeof nome, "%s-6-reaberta-marca.png", saida);
  gravar(nome);

  // AUTOMATICO: volta tudo. Esquerda ate o primeiro e OK.
  { int k; for (k = 0; k < 6; k++) tecla(SDLK_LEFT); }
  tecla(SDLK_RETURN);
  assert(arteesc_fundo("ensaio:1") == NULL);
  quadros(60);
  assert(!strcmp(arteDe(idx), "deploy/app/art/07.jpg"));

  // VOLTAR NAO GRAVA NADA.
  abrirTroca();
  injetar();
  tecla(SDLK_RIGHT);
  quadros(30);
  tecla(SDLK_ESCAPE);
  assert(!trocaarte_aberto());
  assert(arteesc_fundo("ensaio:1") == NULL);
  printf("trocaarte_shot: tudo certo\n");

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
