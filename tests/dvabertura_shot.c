// A ABERTURA DO DOLBY VISION E A ABERTURA NORMAL (dvtela.h, player.c).
//
//   bash tests/dvabertura_shot.sh /tmp/nv-dvabertura
//
// Prova, com o player inteiro e o pipeline simulado, em Centro e Canto e nos tres
// fundos (Off / Arte / Desfocado):
//   1. a abertura do DV e a normal PIXEL a PIXEL fora da marca DOLBY VISION e da
//      linha de passo (mesmo logo, mesmo anel, mesma arte, mesmo veu);
//   2. dentro dessas duas zonas ha desenho (a marca e a linha existem);
//   3. normal -> DV e DV -> normal no meio da abertura: o quadro N+1 nao salta
//      em relacao ao N fora das zonas do DV (com animacoes reduzidas, zero; com
//      elas, no maximo o que ja muda numa troca de fonte normal -> normal).
// E salva as capturas PNG 1920x1080 de cada caso.
#include "catalogo.h"
#include "nvtmp.h"
#include "player.h"
#include "plrilha.h"
#include "dvtela.h"
#include "anim.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "episodios.h"
#include "streams.h"
#include "video.h"
#include "idioma.h"
#include "artehero.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Fraca: numa arvore sem os encaixes a zona e vazia e tudo conta como diferenca.
__attribute__((weak)) void dvtela_abertura_zonas(float marca[4], float linha[4]) {
  memset(marca, 0, 4 * sizeof *marca); memset(linha, 0, 4 * sizeof *linha);
}

enum { LW = 1920, LH = 1080, K = 150, MAX_FORA = 0, PAD = 6, LIMIAR = 3 };
static GLuint fbo, fboTex;
static Uint32 relogio = 100000;
static VideoSimulacao V;
static int falhas;
static unsigned char *quadro_[6];   // 0/1 par normal+dv; 2/3 troca; 4 base

static void quadros(int n) {
  int i;
  for (i = 0; i < n; i++) {
    relogio += 16;
    SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
    video_simular(&V);
    player_atualizar(1.f / 60, relogio);
    episodios_atualizar(1.f / 60);
    stream_folha_atualizar(1.f / 60, relogio); faixas_atualizar(1.f / 60, relogio);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo); glViewport(0, 0, LW, LH);
    glClearColor(.62f, .66f, .74f, 1); glClear(GL_COLOR_BUFFER_BIT);
    player_desenhar(relogio);
    plrilha_desenhar(relogio);
    dvtela_desenhar(relogio);
  }
}

// Copia o quadro (de cima para baixo) e, se pedido, grava o PNG.
static void capturar(unsigned char *rgb, const char *png) {
  SDL_Surface *s = SDL_CreateRGBSurface(0, LW, LH, 24, 0xff, 0xff00, 0xff0000, 0);
  int y;
  glFinish(); glBindFramebuffer(GL_FRAMEBUFFER, fbo); glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, LW, LH, GL_RGB, GL_UNSIGNED_BYTE, s->pixels);
  for (y = 0; y < LH; y++) memcpy(rgb + (size_t)y * LW * 3, (unsigned char *)s->pixels + (size_t)(LH - 1 - y) * s->pitch, LW * 3);
  if (png) {
    SDL_Surface *o = SDL_CreateRGBSurfaceFrom(rgb, LW, LH, 24, LW * 3, 0xff, 0xff00, 0xff0000, 0);
    assert(IMG_SavePNG(o, png) == 0);
    SDL_FreeSurface(o);
  }
  SDL_FreeSurface(s);
}

static void abrir(int dv) {
  if (player_aberto()) player_encerrar();
  memset(&V, 0, sizeof V);
  V.aceita = 1; V.dvCandidato = dv; V.duracao = 7980.0;
  video_simular(&V);
  relogio = 100000;
  player_abrir(0, NULL);
  player_definir_tentativa(1, 1);
  quadros(4);
  player_definir_fonte("https://fonte.invalid/arquivo?id=1");
}
// Troca a fonte no meio da abertura (a troca automatica de fonte do app).
static void trocar(int dv) { V.dvCandidato = dv; video_simular(&V); player_definir_fonte("https://fonte.invalid/arquivo?id=2"); }

static int dentro(float z[4], int x, int y) {
  return z[2] > 0 && x >= z[0] - PAD && x < z[0] + z[2] + PAD && y >= z[1] - PAD && y < z[1] + z[3] + PAD;
}
static void zonas(float m[4], float l[4]) {
  dvtela_abertura_zonas(m, l);
}
// Pixels que mudam fora das duas zonas (*fora) e dentro delas (*em).
static void comparar(const unsigned char *a, const unsigned char *b, int *fora, int *em) {
  float m[4], l[4];
  int x, y, c;
  zonas(m, l);
  *fora = *em = 0;
  for (y = 0; y < LH; y++)
    for (x = 0; x < LW; x++) {
      const unsigned char *p = a + ((size_t)y * LW + x) * 3, *q = b + ((size_t)y * LW + x) * 3;
      int d = 0;
      for (c = 0; c < 3; c++) if (abs(p[c] - q[c]) > LIMIAR) d = 1;
      if (!d) continue;
      if (dentro(m, x, y) || dentro(l, x, y)) (*em)++; else (*fora)++;
    }
}
static void confere(int ok, const char *quando) {
  printf("%s %s\n", ok ? "ok   " : "FALHA", quando);
  if (!ok) falhas++;
}

static void ajustar(int estilo, int fundo, int reduzidas) {
  assert(ajustes_shot_valor("aberturaEstiloLocal", estilo));
  assert(ajustes_shot_valor("aberturaFundoLocal", fundo));
  assert(ajustes_shot_valor("animacoes", reduzidas));
  anim_politica_reduzida = ajustes_animacoes_reduzidas();
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-dvabertura");
  static const char *ESTILO[] = { "centro", "canto" }, *FUNDO[] = { "off", "arte", "desfocado" };
  int e, f, i, k;
  CatItem c;
  char nome[800], msg[200];
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_Window *w = SDL_CreateWindow("Nuvio: dvabertura", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN); assert(w);
  SDL_GLContext gl = SDL_GL_CreateContext(w); assert(gl); SDL_GL_SetSwapInterval(0);
  glGenTextures(1, &fboTex); glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, LW, LH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, LW, LH); gfx_tamanho_alvo(LW, LH); assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1)); tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  { char caminho[700]; FILE *fp;
    snprintf(caminho, sizeof caminho, "%s/ajustes.txt", getenv("NUVIO_DADOS"));
    fp = fopen(caminho, "w"); assert(fp);
    fprintf(fp, "idioma 0\nselected_theme 2\n");
    fclose(fp);
    ajustes_dir(getenv("NUVIO_DADOS")); }
  ajustes_iniciar();
  memset(&c, 0, sizeof c);
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.titulo, sizeof c.titulo, "A Noite dos Espelhos");
  snprintf(c.backdrop, sizeof c.backdrop, "deploy/app/art/19.jpg");
  snprintf(c.logo, sizeof c.logo, "deploy/app/art/logo/04.png");
  cat_definir(&c, 1);
  // Arte e logo ja decodificados, como na pagina do titulo logo antes do Play.
  for (k = 0; k < 600; k++) {
    const char *lg = artehero_logo_sessao(&c), *u = artehero_url(&c);
    GLuint t = u && u[0] ? tex_obter_larg(u, 480) : 0;
    int ok = tex_obter_hero(c.backdrop) && lg && tex_obter_larg_qualquer(lg, 640) && t && gfx_desfocado(t, u);
    tex_bombear(8); SDL_Delay(5);
    if (ok) break;
  }
  for (i = 0; i < 6; i++) quadro_[i] = malloc((size_t)LW * LH * 3);

  // ---- 1 e 2. normal x DV: igual fora da marca e da linha ------------------
  for (e = 0; e < 2; e++)
    for (f = 0; f < 3; f++) {
      int fora, em;
      ajustar(e, f, 0);
      snprintf(nome, sizeof nome, "%s/%s-%s-normal.png", saida, ESTILO[e], FUNDO[f]);
      abrir(0); quadros(K); capturar(quadro_[0], nome);
      snprintf(nome, sizeof nome, "%s/%s-%s-dv.png", saida, ESTILO[e], FUNDO[f]);
      abrir(1); quadros(K); capturar(quadro_[1], nome);
      assert(dvtela_ativa());
      comparar(quadro_[0], quadro_[1], &fora, &em);
      printf("%s/%s: fora das zonas %d px, dentro %d px\n", ESTILO[e], FUNDO[f], fora, em);
      snprintf(msg, sizeof msg, "%s/%s: DV = normal fora da marca e da linha", ESTILO[e], FUNDO[f]);
      confere(fora <= MAX_FORA, msg);
      snprintf(msg, sizeof msg, "%s/%s: a marca e a linha do DV existem", ESTILO[e], FUNDO[f]);
      confere(em > 200, msg);
    }

  // Provider and long-wait content must stay put when DV joins the stack.
  assert(ajustes_shot_valor("aberturaAddonLocal", 1));
  for (e = 0; e < 2; e++) {
    int fora, em;
    ajustar(e, 1, 0);
    abrir(0); player_definir_tentativa(2, 3); quadros(570); capturar(quadro_[0], NULL);
    abrir(1); player_definir_tentativa(2, 3); quadros(570); capturar(quadro_[1], NULL);
    comparar(quadro_[0], quadro_[1], &fora, &em);
    confere(fora == 0, "Provider and eight-second wait retain their positions");
  }
  assert(ajustes_shot_valor("aberturaAddonLocal", 0));

  // ---- 3. a troca no meio da abertura, nos dois sentidos -------------------
  for (e = 0; e < 2; e++)
    for (k = 1; k >= 0; k--) {          // k = animacoes reduzidas
      int base, fN, fD, em, margem = k ? 0 : 40;
      ajustar(e, 1, k);
      abrir(0); quadros(K); capturar(quadro_[4], NULL);
      trocar(0); quadros(120); capturar(quadro_[5], NULL);
      comparar(quadro_[4], quadro_[5], &base, &em);   // a linha de base: normal -> normal
      printf("%s anim=%s: troca normal->normal muda %d px\n", ESTILO[e], k ? "reduzidas" : "ligadas", base);

      abrir(0); quadros(K); capturar(quadro_[2], NULL);
      trocar(1); quadros(120); capturar(quadro_[3], NULL);
      assert(dvtela_ativa());
      comparar(quadro_[2], quadro_[3], &fN, &em);
      printf("%s anim=%s: normal->DV muda %d px fora das zonas\n", ESTILO[e], k ? "reduzidas" : "ligadas", fN);
      snprintf(msg, sizeof msg, "%s anim=%s: normal -> DV sem salto", ESTILO[e], k ? "reduzidas" : "ligadas");
      confere(fN <= base + margem, msg);

      abrir(1); quadros(K); capturar(quadro_[2], NULL);
      trocar(0); quadros(120); capturar(quadro_[3], NULL);
      assert(!dvtela_ativa());
      comparar(quadro_[2], quadro_[3], &fD, &em);
      printf("%s anim=%s: DV->normal muda %d px fora das zonas\n", ESTILO[e], k ? "reduzidas" : "ligadas", fD);
      snprintf(msg, sizeof msg, "%s anim=%s: DV -> normal sem salto", ESTILO[e], k ? "reduzidas" : "ligadas");
      confere(fD <= base + margem, msg);
      snprintf(msg, sizeof msg, "%s anim=%s: DV entry settles", ESTILO[e], k ? "reduzidas" : "ligadas");
      abrir(0); quadros(K); trocar(1); quadros(120); capturar(quadro_[2], NULL);
      quadros(60); capturar(quadro_[3], NULL); comparar(quadro_[2], quadro_[3], &fN, &em);
      confere(fN <= base + margem, msg);
      abrir(1); quadros(K); trocar(0); quadros(120); capturar(quadro_[2], NULL);
      quadros(60); capturar(quadro_[3], NULL); comparar(quadro_[2], quadro_[3], &fD, &em);
      snprintf(msg, sizeof msg, "%s anim=%s: DV exit settles", ESTILO[e], k ? "reduzidas" : "ligadas");
      confere(fD <= base + margem, msg);
    }

  // ---- Detalhada: o cartao de sempre, dos dois lados (so as capturas) ------
  assert(ajustes_shot_valor("telaAberturaLocal", 1));
  ajustar(0, 1, 0);
  snprintf(nome, sizeof nome, "%s/detalhada-normal.png", saida);
  abrir(0); quadros(K); capturar(quadro_[0], nome);
  GfxRect normal, dv;
  assert(plrilha_shot_centro(&normal));
  snprintf(nome, sizeof nome, "%s/detalhada-dv.png", saida);
  abrir(1); quadros(K); capturar(quadro_[1], nome);
  confere(plrilha_shot_centro(&dv) && fabsf(normal.w - dv.w) < 1 &&
          fabsf(normal.h - dv.h) < 1 && fabsf(normal.x - dv.x) < 1 &&
          fabsf(normal.y - dv.y) < 1 && fabsf(dv.w - 680) < 1,
          "Detailed DV preserves the normal 680 px loading card geometry");
  { int fora, em;
    comparar(quadro_[0], quadro_[1], &fora, &em);
    confere(fora == 0, "Detailed DV preserves background, title logo and card pixels");
  }
  confere(dvtela_variante() == DVT_VAR_DETALHADA, "Detalhada: o DV mostra o cartao");

  // Keyboard-selected details must survive every frame of confirmed-playback exit.
  assert(ajustes_shot_valor("telaAberturaLocal", 0));
  ajustar(0, 1, 0);
  abrir(0); quadros(K);
  { SDL_Event ev = {0};
    ev.type = SDL_KEYDOWN; ev.key.keysym.sym = SDLK_DOWN;
    player_evento(&ev);
  }
  trocar(1); quadros(K);
  assert(dvtela_variante() == DVT_VAR_DETALHADA);
  V.pronto = V.tocando = 1; V.pos = 1;
  V.dvFase.caminho = V.dvFase.dvConfirmado = V.dvFase.tocando = 1;
  quadros(1);
  { int stable = 1, frames = 0;
    while (dvtela_visivel() && frames++ < K) {
      quadros(1);
      if (dvtela_visivel() && dvtela_variante() != DVT_VAR_DETALHADA) stable = 0;
    }
    confere(stable && frames > 2 && !dvtela_visivel(),
            "Keyboard-selected details retain their variant through the complete exit fade");
  }

  printf("dvabertura_shot: %s\n", falhas ? "FALHOU" : "ok");
  return falhas ? 1 : 0;
}
