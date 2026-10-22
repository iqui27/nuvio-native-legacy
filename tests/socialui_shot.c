// CAPTURAS DO SOCIAL REDESENHADO (02/10/2026), sem rede e sem interacao:
//
//   fileira "Amigos assistindo" da home (amigosfil.c) com 0, 1 e 3 amigos e
//   com um amigo ao vivo — o rosto em foco com os cartoes abertos e o foco
//   dentro do primeiro cartao;
//   painel da tecla azul: aba Atividade (tela B) e aba Amigos (tela A);
//   perfil do amigo (tela C, amigoperfil.c).
//
// Os dados sao os de exemplo de socialvis.c (socialvis_demo, so com
// -DNV_SOCIALVIS_DEMO); a arte e local (deploy/app/art). recomenda.c e
// INCLUIDO, como em tests/social_shot.c, para semear contatos por dentro.
//
//   bash tests/socialui_shot.sh /tmp/nv-socialui
#define NV_REC_URL "http://127.0.0.1:8799"
// A CONTA TRAKT E O LOGIN DO SIMKL NA TV, so para as linhas de "Contas
// ligadas" (o resto do app continua com os de verdade, sem login).
#define trakt_ativo     shot_trakt_ativo
#define sessao_token_copiar shot_sessao_token_copiar
#define sessao_logada shot_sessao_logada
#define simklauth_token shot_simklauth_token
#include "../src/recomenda.c"
#undef trakt_ativo
#undef sessao_token_copiar
#undef sessao_logada
#undef simklauth_token
static int contasNaTv;
int shot_trakt_ativo(void) { return contasNaTv; }
// sessao.c (#203) entrega o token por copia.
int shot_sessao_token_copiar(char *d, size_t n) { snprintf(d, n, "%s", contasNaTv ? "tok-shot" : ""); return contasNaTv; }
int shot_sessao_logada(void) { return contasNaTv; }
const char *shot_simklauth_token(void) { return contasNaTv ? "simkl-shot" : ""; }
#include "ajustes.h"
#include "amigoperfil.h"
#include "amigosfil.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "salvospainel.h"
#include "avisos.h"
#include "ctxmenu.h"
#include "reacao.h"
#include "recresp.h"
#include "socialvis.h"
#include "tex_cache.h"
#include "text.h"
#include "shot_arte.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// O ALCANCE E O NOME sao os de verdade de recomenda.c (a ponte V2 ligou, e os
// stubs que moravam aqui passaram a redefinir as funcoes dela). O nome do
// perfil e semeado direto na estatica, como os contatos.

static SDL_Window *janela;
static const char *dirDados;
enum { D_HOME = 0, D_PAINEL, D_PERFIL };
static int desenho;

static void gravarBmp(const char *bmp) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  int y;
  assert(pix && s);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(SDL_SaveBMP(s, bmp) == 0);
  SDL_FreeSurface(s);
  free(pix);
  printf("captura: %s\n", bmp);
}

static void quadros(int n, const char *bmp) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    tex_bombear(8);
    home_atualizar(1.0f / 60.0f, agora);
    spainel_atualizar(1.0f / 60.0f, agora);
    ctx_atualizar(1.0f / 60.0f, agora);
    if (desenho == D_PERFIL) amigoperfil_atualizar(1.0f / 60.0f, agora);
    txt_novo_quadro();
    tex_novo_quadro();
    gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (desenho == D_PERFIL) amigoperfil_desenhar(agora);
    else {
      home_desenhar(agora);
      if (desenho == D_PAINEL) spainel_desenhar(agora);
      if (desenho == D_PAINEL && ctx_aberto()) ctx_desenhar(agora);
    }
    if (bmp && i == n - 1) gravarBmp(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  e.type = SDL_KEYUP;
  home_evento(&e);
}

// Leva o foco da aba Amigos a linha do tipo `nome` (spainel_foco_social).
static void focar(const char *nome) {
  SDL_Event e;
  int k;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
  for (k = 0; k < 40; k++) spainel_evento(&e);      // as abas
  e.key.keysym.sym = SDLK_DOWN;
  for (k = 0; k < 60 && strcmp(spainel_foco_social(), nome); k++) spainel_evento(&e);
  if (strcmp(spainel_foco_social(), nome)) { fprintf(stderr, "sem linha %s\n", nome); exit(1); }
}
static void painelTecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  spainel_evento(&e);
  e.type = SDL_KEYUP;
  spainel_evento(&e);
}

static void ajusta(void) {
  char cam[700];
  FILE *a;
  snprintf(cam, sizeof cam, "%s/ajustes.txt", dirDados);
  a = fopen(cam, "w");
  assert(a);
  fprintf(a, "idioma %d\ntrailerHero 1\nhomeLayoutLocal %d\nselected_theme %d\n",
          getenv("NUVIO_SHOT_EN") ? 1 : 0,
          getenv("NV_LAYOUT") ? atoi(getenv("NV_LAYOUT")) : 1,
          getenv("NV_TEMA") ? atoi(getenv("NV_TEMA")) : 9);
  // NUVIO_SHOT_FONTE=3: Montserrat, a fonte da interface da TV do dono.
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(a, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  shot_arte_material(a);   // NUVIO_SHOT_VIDRO=0: o painel no material solido
  fclose(a);
  ajustes_dir(dirDados);
}

static void contato(int i, const char *id, const char *nome, const char *av, const char *orig) {
  memset(&contatos[i], 0, sizeof contatos[i]);
  snprintf(contatos[i].id, sizeof contatos[i].id, "%s", id);
  snprintf(contatos[i].nome, sizeof contatos[i].nome, "%s", nome);
  snprintf(contatos[i].avatar, sizeof contatos[i].avatar, "%s", av);
  snprintf(contatos[i].origem, sizeof contatos[i].origem, "%s", orig);
}

// Uma recomendacao recebida de Pedro, de um titulo que eu ja comecei (o
// catalogo tem progresso nele): prova a cadeia "Voce comecou › ...".
static void semearRec(void) {
  RecItem *r = &itens[0];
  memset(r, 0, sizeof *r);
  r->id = 1; r->criado = (long long)time(NULL) - 7200;
  snprintf(r->de, sizeof r->de, "nuvio:pedro");
  snprintf(r->deNome, sizeof r->deNome, "Pedro");
  snprintf(r->deAvatar, sizeof r->deAvatar, "deploy/app/art/elenco/00_0.jpg");
  snprintf(r->imdb, sizeof r->imdb, "tt9000002");
  snprintf(r->tipo, sizeof r->tipo, "movie");
  snprintf(r->titulo, sizeof r->titulo, "Titulo 2");
  snprintf(r->poster, sizeof r->poster, "deploy/app/art/poster/22.jpg");
  r->modelo = -1;
  snprintf(r->texto, sizeof r->texto, "Terminei, sua vez");
  r->nota = 81; r->visto = 1;
  nItens = 1;
}

// Leva o foco a fileira de amigos (a segunda: Continuar e ela).
static void irFileira(void) {
  int r;
  home_ir_topo();
  for (r = 0; r < 14; r++) tecla(SDLK_UP);
  quadros(20, NULL);
  tecla(SDLK_DOWN);
  tecla(SDLK_DOWN);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-socialui/s";
  static CatItem itens[40];
  static CatFileira fils[3];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, cen;
  static const int CENARIOS[] = { 0, 1, 3, 4 };

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-socialui-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  janela = SDL_CreateWindow("Nuvio: social", 0, 0, 1920, 1080,
                            SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(janela);
  gl = SDL_GL_CreateContext(janela);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(192);
  snprintf(cache, sizeof cache, "%s/cache", dirDados);
  tex_cache_dir(cache);
  gfx_icones_dir("deploy/app/art");
  ajusta();
  assert(home_iniciar("deploy/app/art"));
  recomenda_iniciar();
  aparecer = REC_APARECER_SIM;
  snprintf(meuNome, sizeof meuNome, "%s", "Henrique");
  snprintf(meuCodigo, sizeof meuCodigo, "%s", "uv8scv");

  // Duas fileiras de catalogo: Continuar e Populares. A de amigos entra
  // sozinha (home.c poe a fileira social na posicao 1).
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 20; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Titulo %d", i);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", 20 + i);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", 20 + i);
    if (i < 6) { c->progresso = 30 + i * 8; c->restanteMin = 40; }
  }
  snprintf(fils[0].chave, sizeof fils[0].chave, "continue_watching");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Continuar assistindo");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  fils[0].ini = 0; fils[0].n = 6;
  snprintf(fils[1].chave, sizeof fils[1].chave, "pop_movie");
  snprintf(fils[1].titulo, sizeof fils[1].titulo, "Popular - Filme");
  snprintf(fils[1].tipo, sizeof fils[1].tipo, "movie");
  fils[1].ini = 6; fils[1].n = 14;
  cat_definir_tudo(itens, 20, fils, 2);
  quadros(30, NULL);

  for (i = 0; i < (int)(sizeof CENARIOS / sizeof *CENARIOS); i++) {
    cen = CENARIOS[i];
    socialvis_demo(cen);
    // Os amigos tambem sao CONTATOS (a aba Amigos lista contatos).
    nContatos = 0;
    if (cen >= 1) contato(nContatos++, "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", "nuvio");
    // NUVIO_SHOT_CRIADOR=1: Pedro com apelido e o selo de criador que o
    // servidor manda (/v1/contatos), para ver o selo na linha e no perfil.
    if (cen >= 1 && getenv("NUVIO_SHOT_CRIADOR")) {
      snprintf(contatos[nContatos - 1].apelido, sizeof contatos[0].apelido, "pedro cine");
      snprintf(contatos[nContatos - 1].selo, sizeof contatos[0].selo, "criador");
    }
    if (cen >= 3) {
      contato(nContatos++, "nuvio:marina", "Marina", "", "nuvio");
      contato(nContatos++, "trakt:vlern", "vlern", "", "trakt");
    }
    socialvis_definir_feed(NULL, 0);   // so para forcar a releitura dos contatos
    socialvis_demo(cen);
    desenho = D_HOME;
    irFileira();
    quadros(110, NULL);
    snprintf(bmp, sizeof bmp, "%s-fileira-%d.bmp", saida, cen);
    quadros(1, bmp);
    if (cen >= 1) {
      tecla(SDLK_RIGHT);
      quadros(60, NULL);
      snprintf(bmp, sizeof bmp, "%s-fileira-%d-cartao.bmp", saida, cen);
      quadros(1, bmp);
      printf("dentro=%d\n", amigosfil_dentro());
      tecla(SDLK_LEFT);
    }
    if (cen == 3) {
      tecla(SDLK_RIGHT); tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);   // para o 2o rosto
      tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
      quadros(90, NULL);
      snprintf(bmp, sizeof bmp, "%s-fileira-%d-rosto2.bmp", saida, cen);
      quadros(1, bmp);
    }
  }

  // --- painel (cenario 4: ao vivo, novidade, rec mandada) ---
  desenho = D_PAINEL;
  semearRec();
  spainel_abrir();
  spainel_ir_aba(1);
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-atividade.bmp", saida);
  quadros(1, bmp);
  // A PERGUNTA DO NIVEL, que vem antes da lista enquanto nao respondida.
  spainel_ir_aba(2);
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-alcance.bmp", saida);
  quadros(1, bmp);
  { SDL_Event e;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
    spainel_evento(&e);
    e.key.keysym.sym = SDLK_RETURN;
    spainel_evento(&e); }               // "So meus amigos"
  printf("alcance respondido: %d\n", recomenda_alcance());
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-amigos.bmp", saida);
  quadros(1, bmp);
  // --- AMIGOS 06/10: as duas portas, a previa, o nivel em segmentos -------
  focar("encontrar");
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-portas.bmp", saida);
  quadros(1, bmp);
  focar("previa");
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-previa.bmp", saida);
  quadros(1, bmp);
  focar("alcance");
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-alcance-seg.bmp", saida);
  quadros(1, bmp);
  painelTecla(SDLK_RIGHT);                           // cursor em "Amigos de amigos"
  quadros(40, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-alcance-cursor.bmp", saida);
  quadros(1, bmp);
  painelTecla(SDLK_RETURN);
  printf("alcance pelo segmento: %d\n", recomenda_alcance());
  assert(recomenda_alcance() == REC_ALCANCE_AMIGOS2);
  painelTecla(SDLK_LEFT);
  painelTecla(SDLK_RETURN);                          // de volta a "Amigos"
  assert(recomenda_alcance() == REC_ALCANCE_AMIGOS);
  // --- CONTAS LIGADAS: Trakt e Simkl para ligar, Letterboxd para ligar ------
  SDL_LockMutex(mtx);
  identRecurso = 1;
  identTrakt[0] = 0;
  SDL_UnlockMutex(mtx);
  contasNaTv = 1;
  spainel_ir_aba(1); quadros(5, NULL); spainel_ir_aba(2);   // reconstroi com as contas
  quadros(30, NULL);
  focar("trakt");
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-contas-antes.bmp", saida);
  quadros(1, bmp);
  // LIGANDO: o OK manda o pedido; o fio (de mentira) ainda nao respondeu.
  SDL_LockMutex(mtx); identOp = REC_IDENT_OP_INDO; SDL_UnlockMutex(mtx);
  quadros(20, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-contas-ligando.bmp", saida);
  quadros(1, bmp);
  // O SERVIDOR RESPONDEU: ligado. Tres quadros da animacao e o fim.
  SDL_LockMutex(mtx);
  identOp = REC_IDENT_OP_OK;
  snprintf(identTrakt, sizeof identTrakt, "%s", "rique-trakt");
  SDL_UnlockMutex(mtx);
  { static const unsigned em[3] = { 180, 420, 640 };
    Uint32 t0;
    int k;
    quadros(1, NULL);                                  // o desenho ve a virada
    t0 = SDL_GetTicks();
    for (k = 0; k < 3; k++) {
      while (SDL_GetTicks() - t0 < em[k]) SDL_Delay(2);
      snprintf(bmp, sizeof bmp, "%s-painel-contas-anim-%d.bmp", saida, k + 1);
      quadros(1, bmp);
    } }
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-contas-depois.bmp", saida);
  quadros(1, bmp);
  // As tres ligadas, com o foco fora delas.
  SDL_LockMutex(mtx);
  identSimklLig = 1;
  snprintf(identLbUsuario, sizeof identLbUsuario, "%s", "henrique");
  SDL_UnlockMutex(mtx);
  quadros(120, NULL);
  focar("aparecer");
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-contas-todas.bmp", saida);
  quadros(1, bmp);
  // CONFIRMAR SEPARAR (1o OK no Trakt ligado) e o conflito, como antes.
  focar("trakt");
  painelTecla(SDLK_RETURN);
  quadros(30, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-ident-confirma.bmp", saida);
  quadros(1, bmp);
  SDL_LockMutex(mtx);
  identTrakt[0] = 0; identOp = REC_IDENT_OP_CONFLITO;
  SDL_UnlockMutex(mtx);
  quadros(30, NULL);
  snprintf(bmp, sizeof bmp, "%s-painel-ident-conflito.bmp", saida);
  quadros(1, bmp);
  SDL_LockMutex(mtx);
  identRecurso = 0; identOp = REC_IDENT_OP_NADA;    // servidor antigo: as linhas somem
  identSimklLig = 0; identLbUsuario[0] = 0;
  SDL_UnlockMutex(mtx);
  contasNaTv = 0;
  quadros(30, NULL);
  // --- W10 (03/10): menu do OK longo nas abas, "Ja assisti" e Assistidas ---
  { SDL_Event e;
    int k;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    (void)k;
    focar("rec");                                     // a rec
    quadros(30, NULL);
    e.key.keysym.sym = SDLK_RETURN; spainel_evento(&e);   // segura OK
    SDL_Delay(760);
    quadros(40, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-amigos-menu.bmp", saida);
    quadros(1, bmp);
    e.type = SDL_KEYUP; ctx_evento(&e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN;
    for (k = 0; k < 2; k++) ctx_evento(&e);           // "Ja assisti" (sem "Mais informações")
    e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-ja-assisti.bmp", saida);
    quadros(1, bmp);
    reacao_evento(&e, 0);                               // Gostei
    quadros(40, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-mensagem.bmp", saida);
    quadros(1, bmp);
    reacao_evento(&e, 0);                               // "Valeu pela dica!"
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-assistidas.bmp", saida);
    quadros(1, bmp);
    printf("rec 1: assistida %d respondida %d\n", recresp_assistida(1), recresp_respondida(1));
    // Atividade: OK longo na primeira linha.
    spainel_ir_aba(1);
    quadros(60, NULL);
    e.key.keysym.sym = SDLK_RETURN; spainel_evento(&e);
    SDL_Delay(760);
    quadros(40, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-atividade-menu.bmp", saida);
    quadros(1, bmp);
    e.key.keysym.sym = SDLK_ESCAPE; ctx_evento(&e);
    quadros(20, NULL); }
  // --- PEDIDOS DE AMIZADE NA ABA AMIGOS (06/10) ---------------------------
  // Tres pedidos na caixa: a secao abre a aba, com Aceitar/Recusar por linha.
  // O OK responde e a linha sai NA HORA (sem esperar o servidor).
  { static const char *ap[3] = { "cine-ana", "rafa-series", "lu" };
    static const char *bio[3] = { "Terror e ficção, sempre", "", "Maratono k-drama" };
    SDL_Event e;
    int k, antes;
    SDL_LockMutex(mtx);
    for (k = 0; k < 3; k++) {
      memset(&pedidosRec[k], 0, sizeof pedidosRec[k]);
      snprintf(pedidosRec[k].pub, sizeof pedidosRec[k].pub, "pub000000%d", k);
      snprintf(pedidosRec[k].apelido, sizeof pedidosRec[k].apelido, "%s", ap[k]);
      snprintf(pedidosRec[k].bio, sizeof pedidosRec[k].bio, "%s", bio[k]);
      snprintf(pedidosRec[k].relacao, sizeof pedidosRec[k].relacao, "recebido");
    }
    snprintf(pedidosRec[0].avatar, sizeof pedidosRec[0].avatar, "deploy/app/art/elenco/01_0.jpg");
    nPedidos = 3;
    SDL_UnlockMutex(mtx);
    spainel_ir_aba(2);
    quadros(60, NULL);
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_UP;
    for (k = 0; k < 20; k++) spainel_evento(&e);
    e.key.keysym.sym = SDLK_DOWN; spainel_evento(&e);   // primeiro pedido
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-pedidos.bmp", saida);
    quadros(1, bmp);
    e.key.keysym.sym = SDLK_RIGHT; spainel_evento(&e);  // Recusar
    quadros(30, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-pedidos-recusar.bmp", saida);
    quadros(1, bmp);
    e.key.keysym.sym = SDLK_LEFT; spainel_evento(&e);   // de volta a Aceitar
    antes = spainel_n_reconstrucoes();
    e.key.keysym.sym = SDLK_RETURN; spainel_evento(&e);
    e.type = SDL_KEYUP; spainel_evento(&e); e.type = SDL_KEYDOWN;
    quadros(40, NULL);
    printf("pedidos: reconstrucoes %d -> %d\n", antes, spainel_n_reconstrucoes());
    snprintf(bmp, sizeof bmp, "%s-painel-pedidos-aceito.bmp", saida);
    quadros(1, bmp);
    // A linha dos Avisos que leva para a aba.
    SDL_LockMutex(mtx); socEstado = REC_SOC_NADA; SDL_UnlockMutex(mtx);
    avisos_atualizar(1.0f / 60.0f, SDL_GetTicks() + 3000u);   // colhe a linha dos pedidos
    assert(avisos_lista_n() >= 1);
    spainel_ir_aba(3);
    quadros(200, NULL);
    snprintf(bmp, sizeof bmp, "%s-painel-avisos-pedidos.bmp", saida);
    quadros(1, bmp); }
  spainel_fechar();
  quadros(30, NULL);

  // --- perfil ---
  desenho = D_PERFIL;
  amigoperfil_abrir("nuvio:pedro");
  // O perfil do servidor de exemplo, pelo MESMO parse da rede (amigoParse): o
  // Pedro respondeu duas recs que eu mandei, uma com mensagem curta.
  { char j[2048];
    long long t = (long long)time(NULL);
    snprintf(j, sizeof j,
      "{\"id\":\"nuvio:pedro\",\"nome\":\"Pedro\",\"grau\":1,\"desde\":%lld,\"origem\":\"codigo\","
      "\"compartilha\":1,\"mes\":{\"mes\":\"2026-10\",\"seg\":111600,\"filmes\":12,\"series\":4},"
      "\"gostou\":[{\"imdb\":\"tt0000101\",\"midia\":\"movie\",\"titulo\":\"Project Hail Mary\","
      "\"poster\":\"deploy/app/art/poster/03.jpg\",\"criado\":%lld}],"
      "\"recs\":[{\"id\":9,\"imdb\":\"tt0000102\",\"tipo\":\"movie\",\"titulo\":\"Duna: Parte Dois\","
      "\"poster\":\"deploy/app/art/poster/05.jpg\",\"criado\":%lld,\"estado\":\"reacao\",\"terminou\":1,"
      "\"reacao\":1,\"resposta\":\"valeu pela dica\",\"respondido\":%lld},"
      "{\"id\":8,\"imdb\":\"tt0000108\",\"tipo\":\"movie\",\"titulo\":\"A Chegada\","
      "\"poster\":\"deploy/app/art/poster/17.jpg\",\"criado\":%lld,\"estado\":\"reacao\",\"terminou\":1,"
      "\"reacao\":0,\"resposta\":\"achei meio lento\",\"respondido\":%lld},"
      "{\"id\":7,\"imdb\":\"tt0000109\",\"tipo\":\"movie\",\"titulo\":\"Blade Runner 2049\","
      "\"poster\":\"deploy/app/art/poster/11.jpg\",\"criado\":%lld,\"estado\":\"entregue\",\"reacao\":null,"
      "\"resposta\":\"\",\"respondido\":0},"
      "{\"id\":6,\"imdb\":\"tt0000110\",\"tipo\":\"movie\",\"titulo\":\"Sicario\","
      "\"poster\":\"deploy/app/art/poster/12.jpg\",\"criado\":%lld,\"estado\":\"reacao\",\"terminou\":1,"
      "\"reacao\":-1,\"resposta\":\"\",\"respondido\":%lld}]}",
      t - 60 * 86400, t - 3600, t - 2 * 86400, t - 3600, t - 3 * 86400, t - 7200, t - 86400,
      t - 5 * 86400, t - 1800);
    assert(amigoParse(j, &amigo));
    temAmigo = 1; amigoEstado = REC_SOC_OK; }
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-perfil-pedro.bmp", saida);
  quadros(1, bmp);
  // A RESPOSTA DE QUEM RECEBEU (W22): foco na fileira "Voce mandou".
  { SDL_Event e; int k;
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN;
    e.key.keysym.sym = SDLK_DOWN;
    quadros(60, NULL);
    for (k = 0; k < 3; k++) amigoperfil_evento(&e);
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-perfil-pedro-resposta.bmp", saida);
    quadros(1, bmp);
    e.key.keysym.sym = SDLK_RIGHT;
    amigoperfil_evento(&e);
    quadros(60, NULL);
    snprintf(bmp, sizeof bmp, "%s-perfil-pedro-resposta2.bmp", saida);
    quadros(1, bmp); }
  // F08: o MESMO perfil com o servidor novo — a comparacao com cobertura
  // (series abaixo do minimo de pares vira "poucos dados").
  { char j[1400];
    long long t = (long long)time(NULL);
    snprintf(j, sizeof j,
      "{\"id\":\"nuvio:pedro\",\"nome\":\"Pedro\",\"grau\":1,\"desde\":%lld,\"origem\":\"codigo\","
      "\"compartilha\":1,\"mes\":{\"mes\":\"2026-10\",\"seg\":111600,\"filmes\":12,\"series\":4},"
      "\"gostou\":[{\"imdb\":\"tt0000101\",\"midia\":\"movie\",\"titulo\":\"Project Hail Mary\","
      "\"poster\":\"deploy/app/art/poster/03.jpg\",\"criado\":%lld}],\"recs\":[],"
      "\"gosto\":{\"total\":12,\"iguais\":9,\"pct\":75,\"filmes\":{\"total\":8,\"iguais\":7},"
      "\"series\":{\"total\":4,\"iguais\":2},\"comum\":{\"filmes\":5,\"series\":2},"
      "\"cobertura\":{\"eu\":20,\"ele\":15},\"generos\":null}}",
      t - 60 * 86400, t - 3600);
    assert(amigoParse(j, &amigo));
    assert(amigo.temCmp && amigo.filmesTotal == 8 && amigo.comumSeries == 2);
    temAmigo = 1; amigoEstado = REC_SOC_OK;
    amigoperfil_abrir("nuvio:pedro");
    temAmigo = 1; amigoEstado = REC_SOC_OK; }
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-perfil-pedro-comparacao.bmp", saida);
  quadros(1, bmp);
  amigoperfil_abrir("nuvio:marina");
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-perfil-marina.bmp", saida);
  quadros(1, bmp);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
