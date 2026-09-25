// SEGURAR OK NO PAINEL DE SALVOS e o ATALHO DO TESTE DE VELOCIDADE (dono,
// 25/09/2026). Dois pedidos pequenos, um teste so, porque os dois precisam da
// mesma janela GL escondida e das mesmas capturas para serem OLHADOS.
//
// O que se cobra no painel:
//   1. toque curto continua abrindo o titulo (agora na soltura, nao no KEYDOWN);
//   2. segurar abre o menu do cartaz NO LIMIAR (NV_HOLD_MS), com o dedo ainda
//      no botao, e em modo painel — por cima dele, com as teclas dele;
//   3. "Remover dos Salvos" tira da lista local E da marca do catalogo (o mesmo
//      OP_LISTA do cartaz da home), o menu sai sozinho e o foco fica na linha
//      SEGUINTE; a lista remonta pela revisao, sem reconstrucao em rajada;
//   4. "Mais informações" entrega o IMDb pelo contrato de sempre
//      (spainel_pediu_abrir) e fecha o painel.
// E no atalho:
//   5. Ajustes › Diagnóstico tem "Teste de velocidade" logo abaixo do
//      diagnostico, e o OK nele e o pedido que app.c le;
//   6. diagnostico_abrir_velocidade + diagnostico_iniciar abre direto no teste
//      e o Voltar do resultado sai da tela.
//
// O Trakt NAO esta vinculado e "Onde o + salva" esta no padrao (Trakt): e o
// caso que o menu do cartaz tratava como falha com a lista local ja escrita.
//
//   bash tests/salvos_segurar.sh [pasta-das-capturas]
#include "ajustes.h"
#include "catalogo.h"
#include "ctxmenu.h"
#include "dados.h"
#include "diagnostico.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "salvos.h"
#include "salvospainel.h"
#include "tex_cache.h"
#include "text.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas;
static void confere(const char *o_que, int ok) {
  printf("  %-66s %s\n", o_que, ok ? "ok" : "FALHOU");
  if (!ok) falhas++;
}

static const char *saida;
static int naTela;   // 0 home + painel, 1 ajustes, 2 diagnostico

// O ROTEADOR, na ordem de app.c: com o painel aberto a tecla e do menu do
// cartaz se ele estiver no ar, senao do painel.
static void rotear(const SDL_Event *e) {
  if (naTela == 1) { ajustes_evento(e); return; }
  if (naTela == 2) { diagnostico_evento(e); return; }
  if (spainel_aberto()) {
    if (ctx_aberto()) ctx_evento(e); else spainel_evento(e);
    return;
  }
  if (ctx_aberto()) ctx_evento(e);
}

static void homeFundo(void *ctx) { (void)ctx; home_desenhar(SDL_GetTicks()); }

static void quadro(void) {
  SDL_Event e;
  Uint32 agora = SDL_GetTicks();
  while (SDL_PollEvent(&e)) rotear(&e);
  tex_bombear(3);
  gfx_novo_quadro();
  tex_novo_quadro();
  gfx_sem_recorte();
  glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  txt_novo_quadro();
  if (naTela == 1) {
    ajustes_atualizar(1.0f / 60.0f, agora);
    ajustes_desenhar(agora);
    return;
  }
  if (naTela == 2) {
    diagnostico_atualizar(1.0f / 60.0f, agora);
    diagnostico_desenhar(agora);
    return;
  }
  home_atualizar(1.0f / 60.0f, agora);
  ctx_atualizar(1.0f / 60.0f, agora);
  spainel_atualizar(1.0f / 60.0f, agora);
  // O desenho de app.c: com o painel na tela o menu vai POR CIMA dele.
  spainel_fundo(!ctx_aberto() || ctx_do_painel(), cat_revisao(), homeFundo, NULL);
  if (spainel_visivel()) { spainel_desenhar(agora); ctx_desenhar(agora); }
  else ctx_desenhar(agora);
}
static void quadros(SDL_Window *w, int n) {
  int i;
  for (i = 0; i < n; i++) { quadro(); SDL_GL_SwapWindow(w); }
}
// Espera `ms` de relogio de verdade desenhando: o limiar do OK longo e medido
// em SDL_GetTicks, como na TV.
static void durante(SDL_Window *w, Uint32 ms) {
  Uint32 t0 = SDL_GetTicks();
  while (SDL_GetTicks() - t0 < ms) { quadro(); SDL_GL_SwapWindow(w); SDL_Delay(8); }
}

// PELA FILA DO SDL, e nao direto no modulo: o observador de pressao longa do
// menu (SDL_AddEventWatch em ctxmenu.c) so ve o que passa pela fila, e e ele
// que desenha a barra de "Segure OK".
static void empurrar(Uint32 tipo, SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = tipo;
  e.key.keysym.sym = k;
  e.key.state = tipo == SDL_KEYDOWN ? SDL_PRESSED : SDL_RELEASED;
  SDL_PushEvent(&e);
}
static void toque(SDL_Window *w, SDL_Keycode k) {
  empurrar(SDL_KEYDOWN, k);
  quadros(w, 2);
  empurrar(SDL_KEYUP, k);
  quadros(w, 2);
}

static void captura(const char *nome) {
  char caminho[700];
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y;
  assert(pix);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  snprintf(caminho, sizeof caminho, "%s/%s", saida, nome);
  assert(IMG_SavePNG(s, caminho) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("  captura: %s\n", caminho);
}
// Desenha ate assentar e captura o ULTIMO quadro, antes da troca de buffer.
static void capturaAssentada(SDL_Window *w, const char *nome, int n) {
  int i;
  for (i = 0; i < n; i++) {
    quadro();
    if (i == n - 1) captura(nome);
    SDL_GL_SwapWindow(w);
  }
}

#define NCAT 60
static CatItem itens[NCAT];
static CatFileira fil;
static const char *TITULOS[] = { "Duna: Parte Dois", "Pobres Criaturas",
  "Anatomia de uma Queda", "Vidas Passadas", "Zona de Interesse",
  "O Menino e a Garça" };

static void salvarLocal(int k) {
  CatItem c;
  memset(&c, 0, sizeof c);
  snprintf(c.imdb, sizeof c.imdb, "tt%07d", 3000000 + k);
  snprintf(c.tipo, sizeof c.tipo, "movie");
  snprintf(c.titulo, sizeof c.titulo, "%s", TITULOS[k]);
  snprintf(c.poster, sizeof c.poster, "deploy/app/art/%02d.jpg", (k * 7) % 40);
  snprintf(c.meta, sizeof c.meta, "%d", 2023 + (k % 2));
  c.nota = 70 + k * 3;
  salvos_definir(&c, 1);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  char id1[24], id2[24], id3[24];
  SDL_Window *w;
  SDL_GLContext gl;
  GLuint fbo, fboTex;
  int i, r0;
  const char *p;
  saida = argc > 1 ? argv[1] : "/tmp";
  if (!dir || !dir[0]) { printf("NUVIO_DADOS ausente; recusando\n"); return 2; }
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) { printf("dados_dir() != NUVIO_DADOS; recusando\n"); return 2; }
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: segurar nos Salvos", 0, 0, 64, 64,
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
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  gfx_icones_dir("deploy/app/art");
  gfx_snap_iniciar(1920, 1080);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  assert(home_iniciar("deploy/app/art"));
  salvos_iniciar();

  // Catalogo de uma fileira. Os dois primeiros titulos salvos TAMBEM estao
  // nele (tem indice, e o reconciliador os marca); os outros so existem na
  // lista local — o caso do painel no arranque, antes da descoberta.
  memset(itens, 0, sizeof itens);
  snprintf(fil.chave, sizeof fil.chave, "teste_movie_top");
  snprintf(fil.titulo, sizeof fil.titulo, "Em alta");
  snprintf(fil.tipo, sizeof fil.tipo, "movie");
  fil.ini = 0; fil.n = NCAT;
  for (i = 0; i < NCAT; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt%07d", i < 2 ? 3000000 + i : 4000000 + i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "%s", i < 2 ? TITULOS[i] : "Outro titulo");
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/%02d.jpg", i % 40);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", (i + 7) % 40);
  }
  cat_definir_tudo(itens, NCAT, &fil, 1);
  for (i = 0; i < 5; i++) salvarLocal(i);
  salvos_reconciliar();
  snprintf(id1, sizeof id1, "tt%07d", 3000000);
  snprintf(id2, sizeof id2, "tt%07d", 3000001);
  snprintf(id3, sizeof id3, "tt%07d", 3000002);
  quadros(w, 30);

  printf("\ntoque curto:\n");
  spainel_abrir();
  durante(w, 400);
  empurrar(SDL_KEYDOWN, SDLK_RETURN);
  quadros(w, 2);
  confere("KEYDOWN sozinho ainda nao abre nada", spainel_aberto() && !ctx_aberto());
  empurrar(SDL_KEYUP, SDLK_RETURN);
  quadros(w, 2);
  p = spainel_pediu_abrir();
  confere("soltar logo abre o titulo focado (o primeiro)", p && !strcmp(p, id1));
  confere("e fecha o painel, sem menu", !spainel_aberto() && !ctx_aberto());
  durante(w, 300);

  printf("\nsegurar OK:\n");
  spainel_abrir();
  durante(w, 400);
  toque(w, SDLK_DOWN);                         // segunda linha: tem indice
  empurrar(SDL_KEYDOWN, SDLK_RETURN);
  durante(w, NV_HOLD_MS / 2);
  captura("salvos-segurar-dica.png");
  confere("no meio do gesto o menu ainda nao abriu", !ctx_aberto());
  durante(w, NV_HOLD_MS / 2 + 120);
  confere("no limiar, com o dedo no botao, o menu abre", ctx_aberto());
  confere("em modo painel (por cima do painel)", ctx_do_painel() && spainel_aberto());
  // A repeticao do controle com o dedo ainda la nao escolhe nada.
  { SDL_Event e; memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; e.key.repeat = 1;
    SDL_PushEvent(&e); }
  quadros(w, 3);
  confere("a repeticao automatica do OK nao escolhe opcao", ctx_aberto());
  empurrar(SDL_KEYUP, SDLK_RETURN);
  capturaAssentada(w, "salvos-segurar-menu.png", 40);
  confere("soltar depois do limiar nao abre o titulo", spainel_pediu_abrir() == NULL);

  printf("\nremover dos salvos:\n");
  r0 = spainel_n_reconstrucoes();
  toque(w, SDLK_DOWN);                         // "Remover dos Salvos"
  capturaAssentada(w, "salvos-segurar-remover.png", 20);
  toque(w, SDLK_RETURN);
  quadros(w, 4);
  confere("saiu da lista local", !salvos_tem(id2));
  confere("e da marca do catalogo (todas as copias)", !cat_imdb_na_lista(id2));
  confere("o menu sai sozinho quando a remocao confirma", !ctx_aberto());
  confere("o painel continua aberto", spainel_aberto());
  confere("a lista remontou sem rajada (1 ou 2 reconstrucoes)",
          spainel_n_reconstrucoes() - r0 >= 1 && spainel_n_reconstrucoes() - r0 <= 2);
  capturaAssentada(w, "salvos-segurar-removido.png", 30);
  toque(w, SDLK_RETURN);
  p = spainel_pediu_abrir();
  confere("o foco ficou na linha SEGUINTE (o terceiro titulo)", p && !strcmp(p, id3));
  durante(w, 300);

  printf("\nmais informacoes:\n");
  spainel_abrir();
  durante(w, 400);
  toque(w, SDLK_DOWN); toque(w, SDLK_DOWN);    // quarta linha (a segunda saiu),
  toque(w, SDLK_DOWN);                         // que so existe na lista local
  empurrar(SDL_KEYDOWN, SDLK_RETURN);
  durante(w, NV_HOLD_MS + 120);
  confere("titulo sem indice no catalogo tambem abre o menu", ctx_do_painel());
  empurrar(SDL_KEYUP, SDLK_RETURN);
  quadros(w, 4);
  toque(w, SDLK_RETURN);                       // primeira opcao
  quadros(w, 3);
  p = spainel_pediu_abrir();
  confere("\"Mais informações\" pede o titulo pelo IMDb",
          p && !strcmp(p, "tt3000004"));
  confere("e fecha o menu e o painel", !ctx_aberto() && !spainel_aberto());
  durante(w, 300);

  printf("\natalho do teste de velocidade:\n");
  naTela = 1;
  ajustes_iniciar();
  quadros(w, 10);
  toque(w, SDLK_ESCAPE);                       // coluna de categorias
  for (i = 0; i < 12; i++) toque(w, SDLK_DOWN); // ate a ultima: Diagnostico
  toque(w, SDLK_RETURN);
  toque(w, SDLK_DOWN);                         // a linha de baixo
  capturaAssentada(w, "ajustes-velocidade.png", 60);
  toque(w, SDLK_RETURN);
  confere("OK em \"Teste de velocidade\" e o pedido que app.c le",
          ajustes_pediu_velocidade() == 1);
  confere("e nao o do diagnostico", ajustes_pediu_diagnostico() == 0);

  naTela = 2;
  diagnostico_abrir_velocidade();
  diagnostico_iniciar();
  capturaAssentada(w, "diagnostico-velocidade.png", 30);
  // Sem addons o teste termina em instantes (resultado "sem medida").
  durante(w, 2500);
  capturaAssentada(w, "diagnostico-velocidade-resultado.png", 10);
  confere("aberto pelo atalho, ainda nao quer sair", !diagnostico_quer_sair());
  toque(w, SDLK_ESCAPE);
  confere("Voltar do resultado sai da tela (volta a Ajustes)", diagnostico_quer_sair());
  diagnostico_iniciar();
  confere("a abertura seguinte e a normal (sem sair sozinha)", !diagnostico_quer_sair());
  diagnostico_encerrar();

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  printf("\n%s\n", falhas ? "FALHOU" : "PASSOU");
  return falhas ? 1 : 0;
}
