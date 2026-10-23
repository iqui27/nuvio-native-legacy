// Captura do cartao de novidades da 1.7 SEM janela visivel — janela GL
// escondida, desenho num FBO, quadro por glReadPixels, como
// tests/novidades160_shot.c. Antes das capturas, confere as regras do cartao
// por evento de tecla. Depois grava as tres cenas (ilha do relogio, Spotlight,
// layout Apple TV) em portugues e em ingles, nos momentos de cada uma, o modal
// e o Spotlight em russo, e com animacoes reduzidas.
#include "novidades170.h"
#include "nvtmp.h"
#include "dados.h"
#include "ajustes.h"
#include "badges.h"
#include "gfx.h"
#include "idiomacod.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int idioma, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", idioma, tema, reduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades170_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

#define MARCA "novidades-170-ui.txt"

static void regras(void) {
  int c;
  // OK com o foco inicial = Abrir o Spotlight.
  dados_apagar(MARCA);
  novidades170_abrir();
  assert(!novidades170_foco_na_previa());
  tecla(SDLK_RETURN);
  assert(!novidades170_aberto());
  assert(novidades170_pedido() == N170_PEDIU_SPOT);
  assert(novidades170_pedido() == N170_PEDIU_NADA);   // consumido
  assert(existe(MARCA));
  // Esquerda + OK = Agora nao; duas esquerdas param na borda.
  dados_apagar(MARCA);
  novidades170_abrir();
  tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades170_aberto());
  assert(novidades170_pedido() == N170_PEDIU_NADA);
  assert(existe(MARCA));
  // Esquerda e direita de volta: o Spotlight de novo.
  dados_apagar(MARCA);
  novidades170_abrir();
  tecla(SDLK_LEFT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(novidades170_pedido() == N170_PEDIU_SPOT);
  // Cima: foco na previa. Direita/OK trocam a cena e NAO fecham; esquerda na
  // primeira volta para a ultima. Baixo devolve o foco aos botoes, no primario.
  dados_apagar(MARCA);
  novidades170_abrir();
  tecla(SDLK_UP);
  assert(novidades170_foco_na_previa());
  c = novidades170_cena();
  tecla(SDLK_RIGHT);
  assert(novidades170_cena() == (c + 1) % novidades170_cenas());
  tecla(SDLK_RETURN);
  assert(novidades170_aberto());
  assert(novidades170_cena() == (c + 2) % novidades170_cenas());
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  assert(novidades170_cena() == novidades170_cenas() - 1);
  tecla(SDLK_DOWN);
  assert(!novidades170_foco_na_previa());
  tecla(SDLK_RETURN);
  assert(novidades170_pedido() == N170_PEDIU_SPOT);
  // Voltar = Agora nao, e grava a marca.
  dados_apagar(MARCA);
  novidades170_abrir();
  tecla(SDLK_ESCAPE);
  assert(!novidades170_aberto());
  assert(novidades170_pedido() == N170_PEDIU_NADA);
  assert(existe(MARCA));
  // Com a marca gravada, primeira_vez nao abre.
  novidades170_primeira_vez();
  assert(!novidades170_aberto());
  puts("PASS: regras do cartao da 1.7");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades170_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades170_desenhar(SDL_GetTicks());
  glFinish();
}

static void gravar(const char *nome) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
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
  printf("captura: %s\n", nome);
}

// Abre o cartao na cena `c`, com o relogio dela em `t` segundos, espera arte e
// texto (quadros parados, decode de verdade no fio do cache) e grava. `andar`
// segundos a mais correm em quadros de 1/60 s (para pegar uma passagem).
static void captura(const char *saida, const char *sufixo, int c, float t, int previa, float andar) {
  char nome[800];
  int i, n = (int)(andar * 60.0f + 0.5f);
  novidades170_abrir();
  if (previa) tecla(SDLK_UP);
  novidades170_ir(c, t);
  for (i = 0; i < 160; i++) { quadro(0.0f); SDL_Delay(4); }
  for (i = 0; i < n; i++) quadro(1.0f / 60.0f);
  for (i = 0; i < 6; i++) { quadro(0.0f); SDL_Delay(2); }
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, sufixo);
  gravar(nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-novidades170");
  const char *so = getenv("N170_SO");   // "pt", "en"...: so um grupo de capturas
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
    char suf[64];
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novidades170_dir("deploy/app/art");

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades170-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
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
  tex_iniciar(96);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");
  badges_carregar("deploy/app/art");   // as marcas de formato (a home faz no app)

  regras();

  // CADA DESCRICAO CABE NUMA LINHA, nos 30 idiomas. Com duas a lista ainda se
  // arruma (medirLista), mas corta com reticencias quando falta altura: aqui
  // se ve qual texto precisa encurtar. So avisa; N170_ESTRITO=1 reprova.
  { int lg, i, largas = 0;
    for (lg = 0; lg < IDIOMA_N; lg++) {
      ajustesDeTeste(lg, 0, 2);
      for (i = 0; i < novidades170_itens(); i++) {
        int lim = 0;
        const char *nome = NULL;
        int w = novidades170_item_largura(i, &lim, &nome);
        if (w > lim) { printf("larga: idioma %d, %s: %d > %d\n", lg, nome, w, lim); largas++; }
      }
    }
    printf("descricoes em duas linhas: %d\n", largas);
    if (getenv("N170_ESTRITO")) assert(largas == 0); }

  // Tema fixo (2) nas capturas; o de animacoes reduzidas usa outro realce.
  // Cada cena num instante "cheio" e os momentos que tem texto apertado: a
  // pilula com o episodio, o modal, o foco no "Salvos ›", o painel, a volta, a
  // estreia; o Spotlight vazio, a cada letra e com o foco na lista; a barra e
  // o carrossel.
  assert(novidades170_cenas() == 3);
  { static const struct { const char *suf; int c; float t; } MOM[] = {
      { "0-relogio", 0, 0.3f }, { "0-vivo", 0, 1.6f }, { "0-tecla", 0, 2.55f },
      { "0-crescendo", 0, 3.0f }, { "0-modal", 0, 4.4f }, { "0-modal-salvos", 0, 6.2f },
      { "0-painel-nascendo", 0, 6.5f }, { "0-painel", 0, 8.0f }, { "0-recolhendo", 0, 9.5f },
      { "0-de-volta", 0, 10.2f }, { "0-estreia", 0, 12.0f },
      { "1-vazio", 1, 1.0f }, { "1-m", 1, 2.2f }, { "1-ma", 1, 3.2f }, { "1-mar", 1, 4.6f },
      { "1-lista", 1, 5.8f }, { "1-desce", 1, 7.5f },
      { "2-barra", 2, 2.75f }, { "2-passagem", 2, 4.65f }, { "2-carrossel", 2, 7.4f },
      { "2-andando", 2, 8.8f },
    };
    static const int LG[2] = { IDIOMA_PT, IDIOMA_EN };
    static const char *const NOME[2] = { "pt", "en" };
    int l, i;
    for (l = 0; l < 2; l++) {
      if (so && strcmp(so, NOME[l])) continue;
      ajustesDeTeste(LG[l], 0, 2);
      for (i = 0; i < (int)(sizeof MOM / sizeof *MOM); i++) {
        snprintf(suf, sizeof suf, "%s-%s", NOME[l], MOM[i].suf);
        captura(saida, suf, MOM[i].c, MOM[i].t, 0, 0.0f);
      }
    }
    if (!so || !strcmp(so, "pt")) {
      captura(saida, "pt-previa", 1, 4.6f, 1, 0.0f);
      // No meio da passagem da cena 0 para a 1.
      captura(saida, "pt-passagem", 0, 13.05f, 0, 0.45f);
    } }
  if (!so || !strcmp(so, "ru")) {
    ajustesDeTeste(IDIOMA_RU, 0, 2);
    captura(saida, "ru-0-modal", 0, 4.4f, 0, 0.0f);
    captura(saida, "ru-1-mar", 1, 4.6f, 0, 0.0f);
  }
  if (!so || !strcmp(so, "reduzido")) {
    ajustesDeTeste(IDIOMA_PT, 1, 5);
    captura(saida, "reduzido", 0, 0.0f, 0, 0.0f);
  }

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 1.7 gravadas.");
  return 0;
}
