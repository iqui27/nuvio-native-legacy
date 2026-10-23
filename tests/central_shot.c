// CAPTURAS DA CENTRAL DE CONTROLE (central.h), sem rede e sem TV: a pilula do
// relogio em repouso, a barra de "segure", quadros do meio da transformacao da
// pilula em painel, o painel aberto (solido e vidro) e a edicao. O relogio
// anda em passos fixos de 16 ms para os quadros do meio serem sempre os
// mesmos. NUVIO_DADOS e temporario (ver o .sh). Saida: <prefixo>-N-*.bmp.
#include "central.h"
#include "nvtmp.h"
#include "ajustes.h"
#include "ajustes_ux.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "anim.h"
#include "ilha.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static Uint32 relogio;

static void salvar(const char *nome) {
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
  printf("captura: %s (t=%.2f)\n", nome, ilha_corpo_t());
}

// `n` quadros de 16 ms; `segura` > 0 poe a barra de segurar na pilula.
static void quadros(int n, float segura) {
  int i;
  for (i = 0; i < n; i++) {
    // O relogio do quadro anda 16 ms e o de verdade espera por ele: a
    // atividade da ilha conta pelo SDL_GetTicks.
    if (!relogio) relogio = SDL_GetTicks();
    relogio += 16u;
    while ((Sint32)(SDL_GetTicks() - relogio) < 0) SDL_Delay(1);
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    tex_bombear(6);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, 1920, 1080);
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_ui_fundo();
    if (segura > 0.0f) ilha_atividade("Segure para abrir a central", segura);
    central_atualizar(0.016f, relogio);
    ilha_relogio_visivel(1);
    ilha_posicionar(1);
    ilha_desenhar(relogio);
  }
}
static void captura(const char *nome, int n) { quadros(n, 0.0f); salvar(nome); }

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-central");
  const char *dir = getenv("NUVIO_DADOS");
  char nome[600], arq[700];
  SDL_Window *w;
  SDL_GLContext gl;
  FILE *f;
  assert(dir && *dir);
  snprintf(arq, sizeof arq, "%s/ajustes.txt", dir);
  f = fopen(arq, "w"); assert(f);
  fprintf(f, "idioma 0\nselected_theme 2\ndolbyVision 0\n");
  fclose(f);
  ajustes_dir(dir);

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: central", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
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
  ajustes_recursos("deploy/app/art");
  gfx_icones_dir("deploy/app/art");
  ajustes_ui_arte(21);
  tex_iniciar(64);

  quadros(40, 0.0f);                        // texto e icones sobem
  snprintf(nome, sizeof nome, "%s-01-pilula.bmp", saida);
  captura(nome, 30);
  quadros(20, 0.55f);
  snprintf(nome, sizeof nome, "%s-02-segurando.bmp", saida);
  salvar(nome);

  // TRES ATALHOS: o painel curto.
  central_teste_lista("dolbyVision\nqualidade\ntrailerAuto\n");
  central_teste_foco(0, 0);                  // abre: a pilula estica
  snprintf(nome, sizeof nome, "%s-03-morfo-a.bmp", saida);
  captura(nome, 6);
  snprintf(nome, sizeof nome, "%s-04-morfo-b.bmp", saida);
  captura(nome, 8);
  snprintf(nome, sizeof nome, "%s-05-aberta-3.bmp", saida);
  captura(nome, 80);

  // OITO: cresce na mola, sem fechar.
  central_teste_lista("qualidade\ndolbyVision\ndolbyAtmos\nescolherFonteManual\n"
                      "trailerAuto\nesmaecerLocal\nvidroLocal\nselosColoridosLocal\n");
  snprintf(nome, sizeof nome, "%s-06-crescendo.bmp", saida);
  captura(nome, 8);
  snprintf(nome, sizeof nome, "%s-07-aberta-8.bmp", saida);
  captura(nome, 80);

  central_teste_tocando("Severance", "T2 E3 · 2160p · Dolby Vision · Atmos");
  central_teste_lista(NULL);
  central_teste_foco(3, 0);
  snprintf(nome, sizeof nome, "%s-08-tocando.bmp", saida);
  captura(nome, 80);
  // ESTATISTICAS (mockup stats-velocidade, tela 1): com video, o primeiro
  // botao e o das estatisticas ao vivo, em foco.
  central_teste_estatisticas(1);
  central_teste_tocando("Duna: Parte Dois", "2160p · HDR10 · 42,8 Mbps agora");
  central_teste_lista("dolbyVision\nqualidade\ntrailerAuto\nesmaecerLocal\nbrilhoPlayerLocal\n");
  central_teste_foco(0, 0);
  snprintf(nome, sizeof nome, "%s-08b-estatisticas.bmp", saida);
  captura(nome, 80);
  central_teste_estatisticas(0);
  central_teste_lista(NULL);
  central_teste_tocando(NULL, NULL);

  ajustes_definir_vidro(1);
  central_teste_foco(-1, 0);
  snprintf(nome, sizeof nome, "%s-09-vidro-editar.bmp", saida);
  captura(nome, 60);

  central_teste_foco(-1, 1);
  snprintf(nome, sizeof nome, "%s-10-edicao.bmp", saida);
  captura(nome, 80);

  // COR DE DESTAQUE e os atalhos novos (203): a central de fabrica agora traz
  // "Cor"; o seletor e a lista com os novos, o acento Oceano ja aplicado.
  ajustes_definir_vidro(0);
  central_teste_lista(NULL);
  central_teste_foco(5, 0);
  snprintf(nome, sizeof nome, "%s-13-padrao-com-cor.bmp", saida);
  captura(nome, 60);
  central_teste_lista("corDestaque\nfonteAutoLocal\nfonteReporLocal\nlegendaForcadaLocal\n"
                      "legendaSyncAudioLocal\nqualidade\ntrailerAuto\nesmaecerLocal\n");
  central_teste_foco(0, 0);
  snprintf(nome, sizeof nome, "%s-14-novos-atalhos.bmp", saida);
  captura(nome, 60);
  central_teste_cor(10);
  snprintf(nome, sizeof nome, "%s-15-seletor-cor.bmp", saida);
  captura(nome, 60);
  ajustes_rapido_cor_definir(15);   // o que o OK faz: o painel recolore
  central_teste_cor(10);
  central_teste_lista(NULL);
  snprintf(nome, sizeof nome, "%s-16-seletor-oceano.bmp", saida);
  captura(nome, 40);
  central_teste_foco(-1, 1);
  central_teste_foco(0, 0);
  snprintf(nome, sizeof nome, "%s-17-oceano-botoes.bmp", saida);
  captura(nome, 40);
  ajustes_rapido_cor_definir(0);

  central_fechar();                          // volta para a pilula
  snprintf(nome, sizeof nome, "%s-11-fechando.bmp", saida);
  captura(nome, 8);
  snprintf(nome, sizeof nome, "%s-12-fechada.bmp", saida);
  captura(nome, 80);

  tex_encerrar(); txt_encerrar(); gfx_encerrar();
  SDL_GL_DeleteContext(gl); SDL_DestroyWindow(w); SDL_Quit();
  puts("PASS: capturas da central");
  return 0;
}
