// CAPTURA DA TELA "ENTRE AMIGOS" (social.c), sem rede. tests/social_shot.c
// fotografa o PAINEL da tecla azul, e nao esta tela; ela so aparecia no
// aparelho. Existe para a correcao da rail fixa (26/09): com
// NUVIO_RAIL=fixa|moderna|recolhida a rail e pintada por cima, como em app.c.
//
// src/social.c e INCLUIDO: `dados` e estatico do modulo e semea-lo por dentro e
// o unico jeito de ter a lista cheia sem o Trakt no ar.
#include "../src/social.c"
#include "rail_shot.h"
#include "dados.h"
#include <SDL2/SDL_image.h>
#include <assert.h>

static GLuint fbo, fboTex;

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < 30; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    gfx_novo_quadro();
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    social_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_PERFIL);
    if (i == 29) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glFinish();
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-social-tela";
  const char *dir = getenv("NUVIO_DADOS");
  static const char *const PESSOAS[5] = { "Marina", "Rafa", "Joana", "Pedro", "Lu" };
  static const char *const ACOES[5] = { "assistiu", "assistiu", "avaliou 9", "assistiu", "começou" };
  static const char *const TITULOS[5] = {
    "Assassinos da Lua das Flores e um Título Longo Demais para Caber",
    "Duna: Parte Dois", "Aftersun", "Tudo em Todo o Lugar ao Mesmo Tempo", "O Brutalista" };
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int i;
  assert(dir && *dir);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  dados_iniciar(dir);
  assert(!strcmp(dados_dir(), dir));
  ajustes_iniciar();
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: captura social", 0, 0, 64, 64,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");

  memset(&dados, 0, sizeof dados);
  snprintf(dados.pessoa.socialNome, sizeof dados.pessoa.socialNome, "Marina Albuquerque");
  snprintf(dados.pessoa.socialSlug, sizeof dados.pessoa.socialSlug, "marina-alb");
  snprintf(dados.local, sizeof dados.local, "Recife, PE");
  snprintf(dados.bio, sizeof dados.bio, "Cinema de autor, séries britânicas e tudo que tenha trilha do Jonny Greenwood.");
  dados.estado = SOCIAL_PRONTO;
  dados.n = 5;
  for (i = 0; i < 5; i++) {
    Atividade *a = &dados.atividades[i];
    snprintf(a->pessoa, sizeof a->pessoa, "%s", PESSOAS[i]);
    snprintf(a->acao, sizeof a->acao, "%s", ACOES[i]);
    snprintf(a->titulo, sizeof a->titulo, "%s", TITULOS[i]);
    snprintf(a->detalhe, sizeof a->detalhe, "Filme · 2024 · Drama");
    snprintf(a->horario, sizeof a->horario, "2%d/09/2026 · 21:%02d", i, 10 + i);
  }
  selecionado = 1;
  snprintf(nome, sizeof nome, "%s-pronto.bmp", saida);
  captura(nome, w);
  dados.estado = SOCIAL_CARREGANDO;
  snprintf(nome, sizeof nome, "%s-carregando.bmp", saida);
  captura(nome, w);
  dados.estado = SOCIAL_PRIVADO;
  snprintf(nome, sizeof nome, "%s-privado.bmp", saida);
  captura(nome, w);
  printf("pronto\n");
  return 0;
}
