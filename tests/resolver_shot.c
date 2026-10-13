// Captura da tela "Resolver um problema" (2.1) em 1920x1080, janela GL escondida
// e desenho num FBO — como tests/novidades148_shot.c. Antes das capturas, confere
// por evento de tecla (RESOLVER_TESTE fixa os fatos do app: sem rede, add-ons nem
// player aqui; AVISOS_TESTE_ENVIO planta o estado do envio do registro):
//   fase 0: foco inicial (banner so com erro recente), OK num cartao testa e vai
//           a fase 1, Voltar fecha;
//   fase 1: Voltar volta a fase 0; "Tentar de novo" fica na fase 1; "Abrir Fontes
//           e addons" fecha e pede RESOLVER_PEDIU_FONTES; Legenda nao tem cadeia;
//   fase 2: o codigo chega e entra na lista gravada; falha mostra "Tentar de
//           novo"; um registro some da lista com OK e do arquivo;
//   D-pad:  em cada estado, todo foco aparece e tem saida (nenhum beco), e a
//           lista de enviados rola ate o ultimo registro.
#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif
#include "resolver.h"
#include "resolverregra.h"
#include "avisos.h"
#include "player.h"
#include "ajustes.h"
#include "anim.h"
#include "layout.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <OpenGL/gl.h>
#endif

static GLuint fbo, fboTex;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

// Um quadro: fundo de mentira (a tela do app embaixo) e o Resolver por cima.
static void quadro(float dt) {
  int i;
  SDL_PumpEvents();
  resolver_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();   // como o app: sem isto o teto de rasterizacao acaba
  tex_novo_quadro();
  tex_bombear(8);      // os icones sobem do fio do cache, que anda no relogio
  gfx_novo_quadro();
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_cor((GfxRect){ 200, 300, 600, 400 }, 0.02f, 0.35f, 0.55f, 1.0f, 1.0f);
  for (i = 0; i < 12; i++)
    gfx_cor((GfxRect){ 960 + i * 70, 700, 60, 160 }, 0.04f, 0.12f + 0.06f * i, 0.2f, 0.8f, 0.9f);
  resolver_desenhar(SDL_GetTicks());
}

static void adiantar(int quadros) {
  int i;
  for (i = 0; i < quadros; i++) { quadro(1000.0f / 60.0f); SDL_Delay(6); }
}

static void captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  adiantar(quadros);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", nome);
}

// ---- Estados de partida.
static void fatos_ok(void) {
  ResolverFatos f;
  memset(&f, 0, sizeof f);
  f.addonsQtd = 3;
  resolver_teste_fatos(&f);
}

static void falha_fonte(void) {
  resolverregra_falha_registrar(RES_T_FONTE, "The source did not answer in time.",
                                "Try another source on the Sources sheet or send a log.", "T2:E5",
                                (long)time(NULL) - 240);
}

static void seed_enviados(int n) {
  ResolverEnviado v[RES_ENVIADOS_MAX];
  static const char *const COD[] = { "4F7K92", "K7QM2X", "9PD3LA", "X2N8RT", "B6H4WC", "M3Z7VJ" };
  char buf[RES_ENVIADOS_MAX * 40];
  long agora = (long)time(NULL);
  int i;
  for (i = 0; i < n; i++) {
    memset(&v[i], 0, sizeof v[i]);
    snprintf(v[i].codigo, sizeof v[i].codigo, "%s", COD[i % 6]);
    v[i].quando = agora - 600L - (long)i * 86400L;   // 10 min, 1 dia, 2 dias...
    v[i].sintoma = i % RES_S_N;
  }
  assert(resolverregra_enviados_serializar(v, n, buf, sizeof buf));
  dados_gravar(RESOLVER_ARQ, buf);
}

static void limpar(void) {
  dados_apagar(RESOLVER_ARQ);
  resolverregra_falha_limpar();
  resolver_teste_esquecer();
  avisos_teste_envio(0, 0, 0, "", 0, 0);
}

static void abrir(void) {
  resolver_abrir();
  assert(resolver_aberto() && resolver_fase() == RESOLVER_FASE_SINTOMA);
}

// Da fase 0 sem banner: desce ate a fileira de baixo e para no chip pedido
// (B_ENVIAR = 51 ou B_LISTA = 52).
static void ir_chip(int id) {
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  if (resolver_foco() != id) tecla(id == 51 ? SDLK_LEFT : SDLK_RIGHT);
  assert(resolver_foco() == id);
}

// ---- Varredura do D-pad. Passeio pseudoaleatorio fixo pelas quatro direcoes a
// partir do estado atual. Confere que: o foco e sempre valido; todo foco
// visitado tem pelo menos uma saida (mexe o foco numa direcao); e cada foco
// visitado volta ao de partida pelas arestas vistas (nenhum beco sem volta).
// `esperados` = quantos focos a fase tem: todos tem de ser alcancados.
static void varredura(int esperados, int faseEsperada) {
  enum { MAXF = 64, PASSOS = 900 };
  static const SDL_Keycode DIR[4] = { SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP };
  int vis[MAXF], nvis = 0, saida[MAXF] = { 0 };
  unsigned char aresta[MAXF][MAXF];
  int i, j, k, p, partida = resolver_foco(), seed = 12345, mudou = 1;
  memset(aresta, 0, sizeof aresta);
  vis[nvis++] = partida;
  for (p = 0; p < PASSOS; p++) {
    int antes = resolver_foco(), depois, a, b;
    seed = seed * 1103515245 + 12345;
    tecla(DIR[(seed >> 16) & 3]);
    assert(resolver_aberto() && resolver_fase() == faseEsperada);
    depois = resolver_foco();
    assert(depois >= 0);
    for (a = 0; a < nvis && vis[a] != antes; a++) {}
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (b == nvis) { assert(nvis < MAXF); vis[nvis++] = depois; }
    if (a == nvis) { assert(nvis < MAXF); vis[nvis++] = antes; }
    for (a = 0; a < nvis && vis[a] != antes; a++) {}
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (antes != depois) { saida[a] = 1; aresta[a][b] = 1; }
  }
  if (nvis != esperados) {
    printf("FALHA: varredura da fase %d achou %d focos, esperava %d:", faseEsperada, nvis, esperados);
    for (i = 0; i < nvis; i++) printf(" %d", vis[i]);
    printf("\n");
    exit(1);
  }
  for (i = 0; i < nvis; i++) assert(saida[i]);
  // fecho transitivo (Floyd-Warshall): todo foco chega a todo foco.
  for (k = 0; k < nvis; k++)
    for (i = 0; i < nvis; i++)
      for (j = 0; j < nvis; j++)
        if (aresta[i][k] && aresta[k][j]) aresta[i][j] = 1;
  for (i = 0; i < nvis; i++)
    for (j = 0; j < nvis; j++)
      if (i != j && !aresta[i][j]) {
        printf("FALHA: fase %d: foco %d nao alcanca %d\n", faseEsperada, vis[i], vis[j]);
        exit(1);
      }
  (void)mudou;
}

static void regras(void) {
  int i;
  // ---- Fase 0 sem erro: foco no primeiro cartao, Voltar fecha.
  limpar(); fatos_ok(); abrir();
  assert(resolver_foco() == 0 && resolver_enviados_n() == 0);
  tecla(SDLK_AC_BACK);
  assert(!resolver_aberto());
  // O Voltar do controle remoto que so manda o scancode tambem fecha.
  limpar(); fatos_ok(); abrir();
  { SDL_Event e;
    memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UNKNOWN;
    e.key.keysym.scancode = NV_SCANCODE_BACK;
    resolver_evento(&e); }
  assert(!resolver_aberto());

  // ---- Fase 0 -> 1: OK num cartao testa; Voltar volta a fase 0.
  limpar(); fatos_ok(); abrir();
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES && resolver_sintoma() == RES_S_NAO_COMECA);
  tecla(SDLK_ESCAPE);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA && resolver_aberto());
  // Cartao por cartao: cada um abre a fase 1 do seu sintoma e volta.
  for (i = 0; i < RES_S_N; i++) {
    limpar(); fatos_ok(); abrir();
    { int g; for (g = 0; g < i % 3; g++) tecla(SDLK_RIGHT);
      for (g = 0; g < i / 3; g++) tecla(SDLK_DOWN); }
    assert(resolver_foco() == i);
    tecla(SDLK_RETURN);
    assert(resolver_fase() == RESOLVER_FASE_TESTES && resolver_sintoma() == i);
    tecla(SDLK_BACKSPACE);
    assert(resolver_fase() == RESOLVER_FASE_SINTOMA);
  }

  // ---- Com erro recente ha o banner e ele e o foco inicial.
  limpar(); fatos_ok(); falha_fonte(); abrir();
  assert(resolver_foco() == 50);   // B_BANNER
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES && resolver_sintoma() == RES_S_NAO_COMECA);
  // O erro de fonte para a cadeia e o painel oferece enviar e tentar de novo.
  tecla(SDLK_RETURN);   // principal = Enviar um registro (sem servidor aqui)
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  tecla(SDLK_ESCAPE);   // Voltar do registro devolve a fase 1
  assert(resolver_fase() == RESOLVER_FASE_TESTES);

  // ---- Offline: o principal e "Tentar de novo" e fica na fase 1.
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 3; f.offline = 1; resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES && resolver_aberto());
  assert(resolver_pedido() == RESOLVER_PEDIU_NADA);

  // ---- Sem add-on: "Abrir Fontes e addons" fecha e pede as Fontes (uma vez).
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 0; resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES);
  tecla(SDLK_RETURN);
  assert(!resolver_aberto());
  assert(resolver_pedido() == RESOLVER_PEDIU_FONTES);
  assert(resolver_pedido() == RESOLVER_PEDIU_NADA);

  // ---- Legenda: sem cadeia, vai direto ao passo a passo.
  limpar(); fatos_ok(); abrir();
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_DOWN);   // 5 = Contas
  tecla(SDLK_LEFT); tecla(SDLK_LEFT); tecla(SDLK_UP);       // 0
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);                      // 4 = Continuar errado
  tecla(SDLK_LEFT);                                         // 3 = Legenda
  assert(resolver_foco() == RES_S_LEGENDA);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_TESTES && resolver_sintoma() == RES_S_LEGENDA);
  tecla(SDLK_ESCAPE);

  // ---- Fase 2: o envio. O codigo que chega entra na lista e no arquivo.
  limpar(); fatos_ok(); abrir();
  ir_chip(51);                                 // Enviar um registro
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  avisos_teste_envio(2, AVISOS_ENVIO_OK, 200, "K7QM2X", 186L * 1024, 2214);
  quadro(0.016f);
  assert(resolver_enviados_n() == 1);
  { char *s = dados_ler(RESOLVER_ARQ);
    assert(s && strstr(s, "K7QM2X"));
    free(s); }
  tecla(SDLK_RETURN);                          // Pronto
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);

  // ---- Envio que falhou oferece "Tentar de novo" e Voltar.
  limpar(); fatos_ok(); abrir();
  ir_chip(51);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  avisos_teste_envio(3, AVISOS_ENVIO_SERVIDOR, 500, "", 186L * 1024, 2214);
  quadro(0.016f);
  assert(resolver_enviados_n() == 0);
  tecla(SDLK_ESCAPE);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);

  // ---- Voltar durante o envio: o codigo que chega depois entra na lista e no arquivo.
  limpar(); fatos_ok(); abrir();
  ir_chip(51);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  tecla(SDLK_ESCAPE);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);
  avisos_teste_envio(2, AVISOS_ENVIO_OK, 200, "K7QM2X", 186L * 1024, 2214);
  quadro(0.016f);
  assert(resolver_enviados_n() == 1);
  { char *s = dados_ler(RESOLVER_ARQ);
    assert(s && strstr(s, "K7QM2X"));
    free(s); }

  // ---- Apagar o ultimo registro persiste: reabrir nao o traz de volta.
  limpar(); fatos_ok(); seed_enviados(1); abrir();
  ir_chip(52);
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);                          // apaga a unica linha
  assert(resolver_enviados_n() == 0);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);
  resolver_teste_esquecer(); fatos_ok(); abrir();
  assert(resolver_enviados_n() == 0);

  // ---- Erro generico de fonte tambem alimenta o Resolver e troca o anterior.
  limpar(); fatos_ok();
  player_erro_fonte_motivo("Antigo", "dica");
  player_erro_fonte();
  { ResolverFalha f;
    assert(resolverregra_falha_recente(&f, (long)time(NULL)));
    assert(f.titulo[0] && strcmp(f.titulo, "Antigo") && !f.dica[0]); }
  abrir();
  assert(resolver_foco() == 50);               // o banner de erro recente
  tecla(SDLK_ESCAPE);

  // ---- Lista dos enviados: OK numa linha remove do arquivo.
  limpar(); fatos_ok(); seed_enviados(3); abrir();
  assert(resolver_enviados_n() == 3);
  ir_chip(52);                                 // Registros enviados
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  assert(resolver_foco() == 200);              // B_LIN0: a primeira linha
  tecla(SDLK_RETURN);
  assert(resolver_enviados_n() == 2);
  { char *s = dados_ler(RESOLVER_ARQ);
    assert(s && !strstr(s, "4F7K92") && strstr(s, "K7QM2X"));
    free(s); }
  tecla(SDLK_ESCAPE);
  assert(resolver_fase() == RESOLVER_FASE_SINTOMA);
  // 30 dias: o que passou da guarda nao volta ao abrir.
  { char buf[128];
    snprintf(buf, sizeof buf, "OLD111 %ld 0\nNEW222 %ld 1\n", (long)time(NULL) - 31L * 86400L, (long)time(NULL) - 60L);
    dados_gravar(RESOLVER_ARQ, buf); }
  resolver_teste_esquecer(); fatos_ok(); abrir();
  assert(resolver_enviados_n() == 1);

  // ---- D-pad. Fase 0 com banner e lista: banner + 6 cartoes + 2 botoes = 9.
  limpar(); fatos_ok(); falha_fonte(); seed_enviados(2); abrir();
  varredura(9, RESOLVER_FASE_SINTOMA);
  limpar(); fatos_ok(); abrir();
  varredura(8, RESOLVER_FASE_SINTOMA);          // sem banner: 6 + 2
  // Fase 1 em cada forma do painel.
  limpar(); fatos_ok(); abrir(); tecla(SDLK_RETURN);   // nada achado: Enviar + Tentar + Outro
  assert(resolver_fase() == RESOLVER_FASE_TESTES);
  varredura(3, RESOLVER_FASE_TESTES);
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 0; resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);                   // Fontes + Tentar + Enviar + Outro
  varredura(4, RESOLVER_FASE_TESTES);
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 3; f.offline = 1; resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);                   // Tentar + Enviar + Outro
  varredura(3, RESOLVER_FASE_TESTES);
  limpar(); fatos_ok(); abrir();
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);   // ainda 2 (so ha 3 colunas)
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);          // Continuar errado... depois Legenda
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);                            // passo a passo: Enviar + Outro
  varredura(2, RESOLVER_FASE_TESTES);
  // Fase 2: so o botao; com lista de 5 (rola ate a ultima): Pronto + 5 linhas.
  limpar(); fatos_ok(); seed_enviados(5); abrir();
  ir_chip(52);
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  varredura(6, RESOLVER_FASE_ENVIO);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-resolver";
  const char *dir = getenv("NUVIO_DADOS");
  char nome[700];
  SDL_Window *w;
  SDL_GLContext gl;

  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("resolver-shot", 0, 0, 64, 64, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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

  // ---- Capturas em 1920x1080.
  // 1: escolher o sintoma, com o erro recente em cima e dois registros enviados.
  limpar(); fatos_ok(); falha_fonte(); seed_enviados(2); abrir();
  snprintf(nome, sizeof nome, "%s-1-sintoma.png", saida);
  captura(nome, 40);

  // 2: os testes, parados na fonte (o ultimo erro).
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-2-testes.png", saida);
  captura(nome, 40);

  // 3: o registro enviado, com o codigo, a lista e o que vai e o que nao vai.
  assert(resolver_foco() == 53);                // principal = Enviar um registro
  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ENVIO);
  avisos_teste_envio(2, AVISOS_ENVIO_OK, 200, "4F7K92", 186L * 1024, 2214);
  snprintf(nome, sizeof nome, "%s-3-registro.png", saida);
  captura(nome, 40);

  // 4: sem erro nenhum, sem registros: a grade sozinha.
  limpar(); fatos_ok(); abrir();
  snprintf(nome, sizeof nome, "%s-4-sintoma-vazio.png", saida);
  captura(nome, 40);

  // 5: sem add-on: o painel manda para Fontes e addons.
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 0; resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-5-testes-addons.png", saida);
  captura(nome, 40);

  // 6: a TV recusou o player (aviso "Em breve" da funcao que ainda nao existe).
  limpar();
  { ResolverFatos f; memset(&f, 0, sizeof f); f.addonsQtd = 3;
    snprintf(f.tvMotivo, sizeof f.tvMotivo, "%s", "A TV recusou abrir o player: o sistema negou a permissão de vídeo.");
    resolver_teste_fatos(&f); }
  abrir(); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-6-testes-tv.png", saida);
  captura(nome, 40);

  // 7: nada achado (o audio aparece "sem teste").
  limpar(); fatos_ok(); abrir(); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-7-testes-nada.png", saida);
  captura(nome, 40);

  // 8: Legenda: sem cadeia, so os passos e o envio.
  limpar(); fatos_ok(); abrir();
  tecla(SDLK_DOWN);                             // linha de baixo, coluna 0
  assert(resolver_foco() == RES_S_LEGENDA);
  tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-8-legenda.png", saida);
  captura(nome, 40);

  // 9: a lista de enviados (5 registros, rolando).
  limpar(); fatos_ok(); seed_enviados(5); abrir();
  ir_chip(52);
  tecla(SDLK_RETURN);
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-9-lista.png", saida);
  captura(nome, 40);

  // 10: o envio que falhou.
  limpar(); fatos_ok(); abrir();
  ir_chip(51);
  tecla(SDLK_RETURN);
  avisos_teste_envio(3, AVISOS_ENVIO_SERVIDOR, 500, "", 186L * 1024, 2214);
  snprintf(nome, sizeof nome, "%s-10-registro-falhou.png", saida);
  captura(nome, 40);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  puts("PASS: resolver: regras conferidas e 10 capturas gravadas.");
  return 0;
}
