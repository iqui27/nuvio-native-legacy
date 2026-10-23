// Captura do cartao de novidades da 2.0.1 SEM janela visivel (janela GL
// escondida, FBO, glReadPixels), como tests/novidades148_shot.c. Antes, as
// regras do cartao por tecla; depois a previa em varios momentos das tres
// cenas, a pagina "Apoie o projeto", ingles e animacoes reduzidas.
#include "novidades201.h"
#include "nvtmp.h"
#include "apoio.h"
#include "dados.h"
#include "ajustes.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "idiomacod.h"
#include "centrallista.h"
#include "idioma.h"
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
  // NUVIO_SHOT_FONTE=3 (TXT_FAMILIA_*): a fonte da interface da TV
  // (Montserrat na TCL do dono), mais larga que a Inter do Mac.
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  fclose(f);
  ajustes_dir(dados_dir());
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novidades201_evento(&e);
}

static int existe(const char *arq) {
  char *s = dados_ler(arq);
  int ok = s != NULL;
  free(s);
  return ok;
}

static void regras(void) {
  // Foco inicial = Continuar: OK vai para a pagina de apoio, sem fechar.
  dados_apagar(N201_ARQ);
  novidades201_abrir();
  assert(novidades201_pagina() == 0);
  tecla(SDLK_RETURN);
  assert(novidades201_aberto() && novidades201_pagina() == 1);
  // Voltar (tecla) volta para as novidades; OK em Concluir fecha.
  tecla(SDLK_ESCAPE);
  assert(novidades201_aberto() && novidades201_pagina() == 0);
  tecla(SDLK_RETURN);
  assert(novidades201_pagina() == 1);
  tecla(SDLK_RETURN);
  assert(!novidades201_aberto());
  assert(novidades201_pedido() == N201_PEDIU_NADA);
  assert(existe(N201_ARQ));
  // Esquerda + OK = Abrir a Central.
  dados_apagar(N201_ARQ);
  novidades201_abrir();
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades201_aberto());
  assert(novidades201_pedido() == N201_PEDIU_CENTRAL);
  assert(novidades201_pedido() == N201_PEDIU_NADA);
  assert(existe(N201_ARQ));
  // Agora nao fecha direto, sem passar pela pagina de apoio.
  dados_apagar(N201_ARQ);
  novidades201_abrir();
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(!novidades201_aberto() && novidades201_pedido() == N201_PEDIU_NADA);
  // Voltar na primeira pagina = Agora nao.
  dados_apagar(N201_ARQ);
  novidades201_abrir();
  tecla(SDLK_ESCAPE);
  assert(!novidades201_aberto() && existe(N201_ARQ));
  // Os dois enderecos preenchidos.
  assert(apoio_n() == 2);
  assert(!strcmp(apoio_url(APOIO_PATREON), "https://www.patreon.com/cw/CraaazyDevs"));
  assert(!strcmp(apoio_url(APOIO_KOFI), "https://ko-fi.com/iqui27"));
  assert(!strcmp(apoio_url_curta(APOIO_PATREON), "patreon.com/cw/CraaazyDevs"));
  puts("PASS: regras do cartao da 2.0.1");
}

static void primeiraVez(void) {
  // Instalacao nova (ou vindo da 1.x): sem a marca do guia da 2.0 o guia abre
  // e este cartao NAO abre, mas fica visto.
  dados_apagar("novidades-20-guia.txt");
  dados_apagar(N201_ARQ);
  novidades201_teste_esquecer();
  novidades201_primeira_vez();
  assert(!novidades201_aberto());
  assert(existe(N201_ARQ));
  puts("PASS: instalacao nova: o cartao da 2.0.1 nao abre e fica visto");
  // Vindo da 2.0.0: o guia ja foi visto, a marca da 2.0.1 nao existe. Abre
  // uma vez; fechar grava a marca.
  dados_gravar("novidades-20-guia.txt", "1\n");
  dados_apagar(N201_ARQ);
  novidades201_teste_esquecer();
  novidades201_primeira_vez();
  assert(novidades201_aberto());
  tecla(SDLK_ESCAPE);
  assert(!novidades201_aberto() && existe(N201_ARQ));
  puts("PASS: vindo da 2.0.0: o cartao abre uma vez e grava a marca");
  // Ja visto: nao abre de novo (em outra sessao do app).
  novidades201_teste_esquecer();
  novidades201_primeira_vez();
  assert(!novidades201_aberto());
  puts("PASS: ja visto: nao abre de novo");
}

static void quadro(float dt) {
  SDL_PumpEvents();
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  novidades201_atualizar(dt, SDL_GetTicks());
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  novidades201_desenhar(SDL_GetTicks());
  glFinish();
}

static void grava(const char *nome) {
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

// Abre o cartao, espera as artes, poe o relogio da previa em `seg` e grava.
// `pagina` 1 = OK em Continuar antes (a troca anda ate o fim).
static void captura(const char *saida, const char *nome, float seg, int pagina) {
  char cam[700];
  int i;
  novidades201_abrir();
  for (i = 0; i < 900 && !novidades201_previa_pronta(); i++) { quadro(0.0f); SDL_Delay(4); }
  assert(novidades201_previa_pronta());
  for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);   // entrada do cartao e da lista
  if (pagina) { tecla(SDLK_RETURN); for (i = 0; i < 40; i++) quadro(1.0f / 60.0f); }
  novidades201_teste_relogio(seg);
  // Quadros parados: icones e texto novos terminam de carregar.
  for (i = 0; i < 12; i++) { quadro(0.0f); SDL_Delay(2); }
  snprintf(cam, sizeof cam, "%s-%s.bmp", saida, nome);
  grava(cam);
}

// Em TODOS os idiomas a lista termina acima do rodape com folga, e com a
// fonte da TV (NUVIO_SHOT_FONTE=3) nenhuma frase precisa de reticencias.
static void cabeEmTodos(void) {
  int idi, i, comCorte = 0;
  for (idi = 0; idi < IDIOMA_N; idi++) {
    int c;
    float folga;
    ajustesDeTeste(idi, 0, 2);
    novidades201_abrir();
    for (i = 0; i < 40; i++) quadro(1.0f / 60.0f);
    folga = novidades201_teste_folga();
    c = novidades201_teste_cortadas();
    if (c) { printf("idioma %d (%s): %d frase(s) com reticencias\n", idi, idioma_iso(idi), c); comCorte++; }
    assert(folga >= 28.0f);
    if (getenv("NUVIO_SHOT_FONTE")) assert(c == 0);
    tecla(SDLK_ESCAPE);
    for (i = 0; i < 20; i++) quadro(1.0f / 60.0f);
  }
  { // Os nomes curtos dos atalhos de fabrica (os que a previa mostra, com o
    // desenho da central): botao de (492 - 40 - 20) / 3 - 24 = 120 px, no
    // degrau menor; nome de varias palavras quebra em duas linhas, entao so
    // conta a palavra mais larga.
    int k, ruins = 0;
    CentralLista fab;
    centrallista_padrao(&fab);
    for (idi = 0; idi < IDIOMA_N; idi++) {
      ajustesDeTeste(idi, 0, 2);
      for (k = 0; k < fab.n; k++) {
        const char *c = i18n(central_catalogo(fab.item[k])->curto);
        char pal[96];
        const char *q = c;
        int cabe = txt_largura(TXT_ILHA_HORA, c) <= 120;
        if (!cabe && strchr(c, ' ')) {   // duas linhas em 16/600: cada palavra cabe?
          cabe = 1;
          while (*q) {
            size_t n = strcspn(q, " ");
            snprintf(pal, sizeof pal, "%.*s", (int)n, q);
            if (txt_largura(TXT_G16B, pal) > 120) cabe = 0;
            q += n; while (*q == ' ') q++;
          }
        }
        if (!cabe) { printf("curto longo (%s): %s\n", idioma_iso(idi), c); ruins++; }
      }
    }
    if (getenv("NUVIO_SHOT_FONTE")) assert(ruins == 0);
  }
  printf("PASS: a lista acaba acima do rodape nos %d idiomas (%d com reticencias)\n", IDIOMA_N, comCorte);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-novidades201");
  const char *dir = getenv("NUVIO_DADOS");
  SDL_Window *w;
  SDL_GLContext gl;
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  novidades201_dir("deploy/app/art");
  primeiraVez();

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("novidades201-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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

  regras();

  ajustesDeTeste(0, 0, 2);
  captura(saida, "pt-central-segurando", 0.62f, 0);
  captura(saida, "pt-central-crescendo", 1.25f, 0);
  captura(saida, "pt-central-aberta", 3.70f, 0);
  captura(saida, "pt-passagem", 4.20f + 0.30f, 0);
  captura(saida, "pt-ajustes", 4.20f + 2.60f, 0);
  captura(saida, "pt-velocidade", 8.40f + 3.20f, 0);
  captura(saida, "pt-apoio", 8.40f + 1.0f, 1);

  ajustesDeTeste(1, 0, 5);
  captura(saida, "en-central", 3.70f, 0);
  captura(saida, "en-apoio", 1.0f, 1);

  ajustesDeTeste(IDIOMA_DE, 0, 2);
  captura(saida, "de-central", 3.70f, 0);
  ajustesDeTeste(IDIOMA_JA, 0, 2);
  captura(saida, "ja-central", 3.70f, 0);
  ajustesDeTeste(IDIOMA_HU, 0, 2);
  captura(saida, "hu-central", 3.70f, 0);
  ajustesDeTeste(IDIOMA_RU, 0, 2);
  captura(saida, "ru-central", 3.70f, 0);

  ajustesDeTeste(0, 1, 2);
  captura(saida, "reduzido", 0.5f, 0);

  cabeEmTodos();

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: capturas da 2.0.1 gravadas.");
  return 0;
}
