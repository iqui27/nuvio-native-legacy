// MINIATURA DO SEEKR NO PLAYER, contra a API de verdade. Fora da suite: precisa
// de janela GL, de rede e da SUA chave, que entra SO pelo ambiente (nunca vai
// para arquivo: este teste chama seekr_definir_chave direto, sem o seekr.txt
// dos Ajustes). Gasta 1 filme e 1 episodio da cota diaria.
//
//   SEEKR_API_KEY=... bash tests/seekr_shot.sh /tmp/nv-seekr/shot
//
// O Mac nao tem pipeline de video (video.c: duracao 0), entao o pedido sai
// daqui com a duracao do arquivo, e o avanco e feito pelas teclas, como na TV.
// NUVIO_SHOT_FITA=1 liga a fita (anterior/atual/seguinte).
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "seekr.h"
#include "dados.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>

static GLuint fbo, fboTex;
static int LW = 1920, LH = 1080;

static void salvar(const char *nome) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y; unsigned char *p, *t;
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  p = s->pixels; t = malloc((size_t)s->pitch);
  for (y = 0; y < LH / 2; y++) {
    memcpy(t, p + y * s->pitch, (size_t)s->pitch);
    memcpy(p + y * s->pitch, p + (LH - 1 - y) * s->pitch, (size_t)s->pitch);
    memcpy(p + (LH - 1 - y) * s->pitch, t, (size_t)s->pitch);
  }
  free(t);
  assert(SDL_SaveBMP(s, nome) == 0);
  SDL_FreeSurface(s);
  printf("captura: %s\n", nome);
}

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();   // o fim do avanco e medido em SDL_GetTicks
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    player_atualizar(1.f / 60, agora);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, agora); faixas_atualizar(1.f / 60, agora);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.10f, .11f, .13f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(agora);
    SDL_Delay(4);
  }
}
static long rssMB(void) { struct rusage u; getrusage(RUSAGE_SELF, &u); return u.ru_maxrss / (1024 * 1024); }

static void tecla(SDL_Keycode k) {
  SDL_Event ev; memset(&ev, 0, sizeof ev);
  ev.type = SDL_KEYDOWN; ev.key.keysym.sym = k; player_evento(&ev);
}

static void rodada(const char *saida, const char *rot, const char *tipo, const char *imdb,
                   int t, int e, long durMs, int passos) {
  char nome[600]; int i;
  CatItem c; memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "%s", tipo);
  snprintf(c.titulo, sizeof c.titulo, "%s", rot);
  snprintf(c.imdb, sizeof c.imdb, "%s", imdb);
  snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
  c.temporada = t; c.episodio = e;
  cat_definir(&c, 1);
  player_abrir(0, NULL);
  if (t) player_definir_episodio(t, e);
  player_erro_fonte(); player_limpar_erro_fonte();
  quadros(20);
  printf("RSS antes: %ld MB\n", rssMB());
  seekr_pedir(imdb, t, e, durMs);
  for (i = 0; i < 150 && seekr_estado() == SEEKR_BUSCANDO; i++) SDL_Delay(100);
  printf("[shot] %s estado=%d\n", rot, seekr_estado());
  assert(seekr_estado() == SEEKR_PRONTO);
  tecla(SDLK_UP); quadros(4);   // da fileira de botoes para a barra
  // Avanco segurado: as repeticoes aceleram (10 s, 30 s, 60 s...).
  for (i = 0; i < passos; i++) { tecla(SDLK_RIGHT); quadros(2); }
  // A folha chega e e recortada em outro fio: espera sem soltar o avanco.
  { Uint32 t0 = SDL_GetTicks(); double cue = 0;
    for (i = 0; i < 2000 && SDL_GetTicks() - t0 < 12000; i++) {
      tecla(SDLK_RIGHT); tecla(SDLK_LEFT); quadros(3);
      if (seekr_quadro(player_posicao_seg(), &cue) && i > 20) break;
    }
    printf("[shot] miniatura em %u ms (quadro dos %.0f s)\n", SDL_GetTicks() - t0, cue); }
  quadros(6);
  printf("RSS depois: %ld MB, posicao %.0f s\n", rssMB(), player_posicao_seg());
  snprintf(nome, sizeof nome, "%s-%s.bmp", saida, t ? "episodio" : "filme");
  salvar(nome);
  player_encerrar(); quadros(30);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-seekr/shot");
  const char *e4 = getenv("NUVIO_SHOT_4K");
  const char *k = getenv("SEEKR_API_KEY");
  if (!k || !*k) { puts("seekr_shot: sem SEEKR_API_KEY, nada a fazer"); return 0; }
  if (e4 && *e4 == '1') { LW = 3840; LH = 2160; }
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: pausa", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  // Idioma e acento pelo caminho de verdade, como em social_shot: portugues e
  // OCEANO (2) por padrao; NUVIO_SHOT_EN=1 e NUVIO_SHOT_THEME=<n> trocam.
  { char caminho[700]; FILE *f;
    const char *en = getenv("NUVIO_SHOT_EN"), *tema = getenv("NUVIO_SHOT_THEME");
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma %d\nselected_theme %d\n", en && *en == '1', tema && *tema ? atoi(tema) : 2);
    // NUVIO_SHOT_FITA=1 liga a fita; NUVIO_SHOT_AJUSTE=<s> a sincronia.
    if (getenv("NUVIO_SHOT_FITA")) fprintf(f, "seekrFitaLocal 0\n");
    if (getenv("NUVIO_SHOT_AJUSTE")) fprintf(f, "seekrAjusteLocal %d\n", atoi(getenv("NUVIO_SHOT_AJUSTE")));
    fclose(f);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  if (getenv("NUVIO_SHOT_VIDRO")) ajustes_definir_vidro(1);

  seekr_definir_chave(k);
  rodada(saida, "The Matrix", "movie", "tt0133093", 0, 0, 8280000L, 25);
  rodada(saida, "Breaking Bad", "series", "tt0903747", 1, 1, 3480000L, 18);
  puts("seekr_shot: ok");
  return 0;
}
