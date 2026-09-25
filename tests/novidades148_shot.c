// Captura do cartao de novidades da 1.4.8 SEM janela visivel — janela GL
// escondida, desenho num FBO, quadro por glReadPixels, como
// tests/novidades142_shot.c. Antes das capturas, confere as regras do cartao
// por evento de tecla. As capturas pegam a previa em tres momentos do ciclo da
// cor (primeiro titulo parado, no meio da passagem, terceiro titulo), em
// portugues e em ingles.
#include "novidades148.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void ajustesDeTeste(int ingles, int reduzidas, int tema) {
  char caminho[700];
  FILE *f;
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  assert(f);
  fprintf(f, "idioma %d\nselected_theme %d\nanimacoes %d\n", ingles, tema, reduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades148_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void regras(void) {
  // OK com o foco inicial = Experimentar a cor viva.
  dados_apagar("novidades-150.txt");
  novidades148_abrir();
  tecla(SDLK_RETURN);
  assert(!novidades148_aberto());
  assert(novidades148_pedido() == N148_PEDIU_COR);
  assert(novidades148_pedido() == N148_PEDIU_NADA);   // consumido
  assert(existe("novidades-150.txt"));
  // Esquerda + OK = teste de velocidade.
  dados_apagar("novidades-150.txt");
  novidades148_abrir();
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(novidades148_pedido() == N148_PEDIU_VELOCIDADE);
  assert(existe("novidades-150.txt"));
  // Duas esquerdas (e uma a mais, que para na borda) + OK = Agora nao.
  dados_apagar("novidades-150.txt");
  novidades148_abrir();
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(novidades148_pedido() == N148_PEDIU_NADA);
  assert(existe("novidades-150.txt"));
  // Voltar = Agora nao, e grava a marca.
  dados_apagar("novidades-150.txt");
  novidades148_abrir();
  tecla(SDLK_RIGHT);   // ja esta na borda direita
  tecla(SDLK_ESCAPE);
  assert(!novidades148_aberto());
  assert(novidades148_pedido() == N148_PEDIU_NADA);
  assert(existe("novidades-150.txt"));
  // Com a marca gravada, primeira_vez nao abre.
  novidades148_primeira_vez();
  assert(!novidades148_aberto());
  // O pedido da cor pousa Ajustes NA LISTA, na linha da cor (Aparencia), e
  // uma vez so: a abertura seguinte volta ao indice de categorias, que e onde
  // a tela abre desde a arquitetura do web (merge da 1.5).
  ajustes_abrir_na_cor();
  ajustes_iniciar();
  assert(!ajustes_foco_no_indice());
  assert(ajustes_opcao_em_foco() > 0);
  { int cor = ajustes_opcao_em_foco();
    ajustes_iniciar();
    assert(ajustes_foco_no_indice());
    ajustes_abrir_na_cor();
    ajustes_iniciar();
    assert(ajustes_opcao_em_foco() == cor); }
  puts("PASS: regras do cartao da 1.4.8");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades148_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades148_desenhar(SDL_GetTicks());
  glFinish();
}

// Abre o cartao, espera as paletas (decode de verdade, no fio do cache) e anda
// `segundos` do ciclo em quadros de 1/60 s antes de gravar.
static void captura(const char *nome, float segundos, int foco) {
  int i, n = (int)(segundos * 60.0f + 0.5f);
  novidades148_abrir();
  for (i = 0; i < foco; i++) tecla(SDLK_LEFT);
  for (i = 0; i < 900 && !novidades148_previa_pronta(); i++) {
    quadro(0.0f);
    SDL_Delay(4);
  }
  assert(novidades148_previa_pronta());
  // Mais alguns quadros parados: texto e icones terminam de subir.
  for (i = 0; i < 30; i++) { quadro(0.0f); SDL_Delay(2); }
  for (i = 0; i < n; i++) quadro(1.0f / 60.0f);
  for (i = 0; i < 10; i++) { quadro(0.0f); SDL_Delay(2); }
  { unsigned char *pix = malloc(1920 * 1080 * 4);
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
    free(pix); }
  printf("captura: %s\n", nome);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-novidades148";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novidades148_dir("deploy/app/art");

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades148-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  gfx_icones_dir("deploy/app/art");

  // Depois do cache de texturas: abrir o cartao ja pede as artes da previa.
  regras();

  // Tema fixo (2): o botao do cartao fica na cor da pessoa, e a previa na
  // cor de cada titulo — as duas coisas lado a lado.
  ajustesDeTeste(0, 0, 2);
  snprintf(nome, sizeof nome, "%s-pt-1.bmp", saida);
  captura(nome, 1.6f, 0);
  snprintf(nome, sizeof nome, "%s-pt-passagem.bmp", saida);
  captura(nome, 4.2f, 0);
  snprintf(nome, sizeof nome, "%s-pt-3.bmp", saida);
  captura(nome, 9.6f, 0);

  ajustesDeTeste(1, 0, 2);
  snprintf(nome, sizeof nome, "%s-en-2.bmp", saida);
  captura(nome, 5.8f, 0);
  snprintf(nome, sizeof nome, "%s-en-velocidade.bmp", saida);
  captura(nome, 9.6f, 1);

  ajustesDeTeste(0, 1, 5);
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome, 4.3f, 2);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 1.4.8 gravadas.");
  return 0;
}
