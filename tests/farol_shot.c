// Capturas da tela do FAROL (issue 19, PLANO 3.2) em PNG 1920x1080 com o app
// inteiro linkado (src/*.c), janela GL ESCONDIDA e desenho num FBO — o padrao
// de tests/resolver_shot.c, portado para Linux (GLES2 + xvfb-run) e macOS.
// Antes das capturas, confere no binario completo o que a captura mostra:
// foco inicial, a grade de 7 sintomas, a Cadeia que anda e entrega o
// Veredito com o foco na acao, e o Voltar. RESOLVER_TESTE: nada sai para a
// rede (o envio so planta estado com AVISOS_TESTE_ENVIO).
//
//   bash tests/farol_shot.sh tests/out/farol
#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif
#include "resolver.h"
#include "resolverregra.h"
#include "farol.h"
#include "farolregra.h"
#include "faroltexto.h"
#include "avisos.h"
#include "ajustes.h"
#include "anim.h"
#include "layout.h"
#include "dados.h"
#include "gfx.h"
#include "gl_compat.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static GLuint fbo, fboTex;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void quadro(float dt) {
  int i;
  SDL_PumpEvents();
  resolver_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 200, 300, 600, 400 }, 0.02f, 0.35f, 0.55f, 1.0f, 1.0f);
  for (i = 0; i < 12; i++)
    gfx_cor((GfxRect){ 960 + i * 70, 700, 60, 160 }, 0.04f, 0.12f + 0.06f * i, 0.2f, 0.8f, 0.9f);
  resolver_desenhar(SDL_GetTicks());
}

static void adiantar(int quadros) {
  for (int i = 0; i < quadros; i++) { quadro(1000.0f / 60.0f); SDL_Delay(2); }
}

// Grava e devolve 1 se a imagem nao e lisa (o desenho saiu de verdade).
static int captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y, variou = 0;
  assert(pix);
  adiantar(quadros);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 1; y < 1920 * 1080 && !variou; y += 997)
    if (memcmp(pix + (size_t)y * 4, pix, 3)) variou = 1;
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
  return variou;
}

static void fatos_ok(void) {
  ResolverFatos f;
  memset(&f, 0, sizeof f);
  f.addonsQtd = 3;
  resolver_teste_fatos(&f);
}

static FarolFatos ff_base(void) {
  FarolFatos f;
  farolregra_fatos_iniciar(&f);
  f.plat = FP_WEBOS;
  f.addonsQtd = 3;
  return f;
}

static void limpar(void) {
  dados_apagar(RESOLVER_ARQ);
  resolverregra_falha_limpar();
  resolver_teste_esquecer();
  avisos_teste_envio(0, 0, 0, "", 0, 0);
}

// Com as animacoes de fabrica a Cadeia anda em quadros: espera o Veredito.
static void ate_veredito(void) {
  for (int i = 0; i < 400 && resolver_fase() == RESOLVER_FASE_CADEIA; i++) quadro(1000.0f / 60.0f);
  assert(resolver_fase() == RESOLVER_FASE_VEREDITO);
}

static void abrir(void) {
  resolver_abrir();
  assert(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
}

static void falha_fonte(void) {
  resolverregra_falha_registrar(RES_T_FONTE, "The source did not answer in time.",
                                "Try another source on the Sources sheet or send a log.", "T2:E5",
                                (long)time(NULL) - 240);
}

static void seed_enviados(int n) {
  ResolverEnviado v[RES_ENVIADOS_MAX];
  static const char *const COD[] = { "4F7K92", "K7QM2X", "9PD3LA" };
  char buf[RES_ENVIADOS_MAX * 40];
  long agora = (long)time(NULL);
  for (int i = 0; i < n; i++) {
    memset(&v[i], 0, sizeof v[i]);
    snprintf(v[i].codigo, sizeof v[i].codigo, "%s", COD[i % 3]);
    v[i].quando = agora - 600L - (long)i * 86400L;
    v[i].sintoma = i % RES_S_N;
  }
  assert(resolverregra_enviados_serializar(v, n, buf, sizeof buf));
  dados_gravar(RESOLVER_ARQ, buf);
}

static void ir_sintoma(int alvo) {
  int idx = -1;
  for (int i = 0; i < RES_S_FAROL_N; i++) if (resolverregra_sintoma_farol(i) == alvo) idx = i;
  assert(idx >= 0);
  for (int i = 0; i < idx / 2; i++) tecla(SDLK_DOWN);
  if (idx % 2) tecla(SDLK_RIGHT);
  assert(resolver_foco() == alvo);
}

static void regras(void) {
  FarolFatos fx;
  // Inicio sem erro: foco no 1o cartao; Voltar fecha.
  limpar(); fatos_ok(); abrir();
  assert(resolver_foco() == resolverregra_sintoma_farol(0));
  tecla(SDLK_AC_BACK);
  assert(!resolver_aberto());
  // Com erro recente: a faixa nasce com o foco e leva ao "nao comeca".
  limpar(); fatos_ok(); falha_fonte(); abrir();
  assert(resolver_foco() == RESOLVER_B_BANNER);
  tecla(SDLK_RETURN);
  ate_veredito();
  assert(resolver_sintoma() == RES_S_NAO_COMECA);
  assert(resolver_foco() == RESOLVER_B_PRINC);
  tecla(SDLK_ESCAPE);
  assert(resolver_fase() == RESOLVER_FASE_INICIO);
  // Com animacao: a Cadeia anda em quadros e entrega o Veredito.
  limpar(); fatos_ok();
  fx = ff_base(); fx.offline = 1;
  resolver_teste_fatos_farol(&fx);
  abrir();
  adiantar(30);
  tecla(SDLK_RETURN);
  if (!ajustes_animacoes_reduzidas()) assert(resolver_fase() == RESOLVER_FASE_CADEIA);
  ate_veredito();
  assert(resolver_foco() == RESOLVER_B_PRINC);
  assert(resolver_teste_veredito() == FAROL_C_REDE_FORA);
  // Cores estranhas pela grade.
  limpar(); fatos_ok();
  fx = ff_base(); fx.fonteDv = 1; fx.tvDv = 0;
  resolver_teste_fatos_farol(&fx);
  abrir();
  ir_sintoma(RES_S_CORES);
  tecla(SDLK_RETURN);
  ate_veredito();
  assert(resolver_teste_veredito() == FAROL_C_IMAGEM_DV);
  assert(resolver_foco() == RESOLVER_B_PRINC);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "farol";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;
  FarolFatos fx;
  int lisas = 0;

  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#if defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
  w = SDL_CreateWindow("farol-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }

  regras();

  // 1: Inicio com a faixa da falha recente e dois registros enviados.
  limpar(); fatos_ok(); falha_fonte(); seed_enviados(2); abrir();
  snprintf(nome, sizeof nome, "%s-1-inicio.png", saida);
  lisas += !captura(nome, 40);

  // 2: Inicio sem falha (sem faixa), foco em "Cores estranhas".
  limpar(); fatos_ok(); abrir();
  ir_sintoma(RES_S_CORES);
  snprintf(nome, sizeof nome, "%s-2-inicio-cores.png", saida);
  lisas += !captura(nome, 40);

  // 3: a Cadeia no meio do caminho (animacao ligada, a luz descendo ate a
  // Fonte: add-ons fixos para a cadeia passar deles).
  limpar(); fatos_ok(); falha_fonte();
  fx = ff_base();
  resolver_teste_fatos_farol(&fx);
  abrir();
  adiantar(30);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_CADEIA || ajustes_animacoes_reduzidas());
  snprintf(nome, sizeof nome, "%s-3-cadeia.png", saida);
  lisas += !captura(nome, 26);
  assert(resolver_fase() == RESOLVER_FASE_CADEIA || ajustes_animacoes_reduzidas());

  // 4: Veredito da fonte (cartao antigo, quebra na Fonte).
  ate_veredito();
  snprintf(nome, sizeof nome, "%s-4-veredito-fonte.png", saida);
  lisas += !captura(nome, 10);

  // 5: Veredito do Farol com "Mandar registro" (TV_FORMATO_REPETE, escala JUNTO).
  limpar(); fatos_ok();
  fx = ff_base(); fx.plat = FP_ANDROID; fx.haFalha = 1; fx.erroIdadeS = 30;
  fx.erro.classe = FC_FORMATO; fx.falhasSeguidas = 2; fx.externoDisponivel = 1;
  resolver_teste_fatos_farol(&fx);
  abrir(); tecla(SDLK_RETURN); ate_veredito();
  assert(resolver_mandar_registro());
  snprintf(nome, sizeof nome, "%s-5-veredito-mandar.png", saida);
  lisas += !captura(nome, 40);

  // 6: Cores estranhas -> IMAGEM_DV.
  limpar(); fatos_ok();
  fx = ff_base(); fx.fonteDv = 1; fx.tvDv = 0;
  resolver_teste_fatos_farol(&fx);
  abrir(); ir_sintoma(RES_S_CORES); tecla(SDLK_RETURN); ate_veredito();
  snprintf(nome, sizeof nome, "%s-6-veredito-cores.png", saida);
  lisas += !captura(nome, 40);

  // 7: sem internet, com a Interface de vidro ligada (vidro 2.1).
  limpar(); fatos_ok();
  fx = ff_base(); fx.offline = 1;
  resolver_teste_fatos_farol(&fx);
  ajustes_definir_vidro(1);
  abrir(); tecla(SDLK_RETURN); ate_veredito();
  snprintf(nome, sizeof nome, "%s-7-veredito-rede-vidro.png", saida);
  lisas += !captura(nome, 40);
  ajustes_definir_vidro(0);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  if (lisas) { printf("FALHA: farol_shot: %d captura(s) lisa(s)\n", lisas); return 1; }
  puts("PASS: farol_shot: regras conferidas e 7 capturas gravadas.");
  return 0;
}
