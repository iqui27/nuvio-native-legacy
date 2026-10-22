// CAPTURAS DO CABECALHO DO PAINEL SOCIAL (07/10/2026): titulo = aba aberta, faixa
// de abas compacta na linha do titulo, folha Editar e aba Agenda. Base: socialui_shot.c.
// (herdado) CAPTURAS DO SOCIAL REDESENHADO (02/10/2026), sem rede e sem interacao:
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
#include "agenda.h"
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
  const char *saida = argc > 1 ? argv[1] : "/tmp/nv-abas/s";
  static CatItem itens[40];
  static CatFileira fils[3];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i, cen;
  

  dados_iniciar("deploy/app/art");
  dirDados = dados_dir();
  if (!dirDados || !strstr(dirDados, "nuvio-abas-shot")) {
    fprintf(stderr, "recuse: NUVIO_DADOS tem de ser a pasta temporaria do teste\n");
    return 1;
  }
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  janela = SDL_CreateWindow("Nuvio: abas", 0, 0, 1920, 1080,
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
    // As quatro seguintes sao series na lista: a Agenda parte delas.
    if (i >= 1 && i <= 4) { snprintf(c->tipo, sizeof c->tipo, "series"); c->naLista = 1; }
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

  (void)cen;
  // Dados: o cenario 4 do demo (amigo ao vivo, novidade), contatos e uma rec.
  socialvis_demo(4);
  nContatos = 0;
  contato(nContatos++, "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", "nuvio");
  contato(nContatos++, "nuvio:marina", "Marina", "", "nuvio");
  socialvis_definir_feed(NULL, 0);
  socialvis_demo(4);
  semearRec();
  aparecer = REC_APARECER_SIM;
  // Agenda: tres series com proximo episodio, uma sem data (nao entra).
  agenda_rede_teste(NULL);
  agenda_definir_hoje("2026-10-07");
  agenda_registrar("tt9000001", "The Last of Us", "deploy/app/art/poster/21.jpg", "Returning Series",
                   3, 4, "Longo, longo tempo", "2026-10-08", "2026-10-01");
  agenda_registrar_extra("tt9000001", "", "", "HBO", "Drama", 55, 3);
  agenda_registrar("tt9000002", "Severance", "deploy/app/art/poster/22.jpg", "Returning Series",
                   2, 9, "Cold Harbor", "2026-10-11", "2026-10-04");
  agenda_registrar_extra("tt9000002", "", "", "Apple TV+", "Drama", 50, 2);
  agenda_registrar("tt9000003", "Slow Horses", "deploy/app/art/poster/23.jpg", "Returning Series",
                   5, 1, "Estreia da temporada", "2026-10-28", "");
  agenda_registrar("tt9000004", "Sem data", "deploy/app/art/poster/24.jpg", "Returning Series",
                   1, 6, "Mesmo dia", "2026-10-08", "");
  desenho = D_PAINEL;
  spainel_abrir();
  // A ABERTURA NA MOLA DA ILHA (movimento.h): dois quadros no meio do caminho.
  quadros(9, NULL);
  snprintf(bmp, sizeof bmp, "%s-0a-abrindo-150ms.bmp", saida);
  quadros(1, bmp);
  quadros(14, NULL);
  snprintf(bmp, sizeof bmp, "%s-0b-abrindo-400ms.bmp", saida);
  quadros(1, bmp);
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-1-salvos.bmp", saida);
  quadros(1, bmp);
  // SETAS COM O FOCO NA LISTA trocam de aba (estilo lista: direita = proxima).
  { int f0 = spainel_foco_indice(), a0 = spainel_aba_atual();
    assert(f0 >= 0 && a0 == 0);
    painelTecla(SDLK_RIGHT);
    assert(spainel_aba_atual() == 1 && spainel_foco_indice() >= 0);
    quadros(40, NULL);
    painelTecla(SDLK_LEFT);
    assert(spainel_aba_atual() == 0 && spainel_foco_indice() >= 0);
    quadros(40, NULL);
    printf("setas na lista: ok\n"); }
  // A TROCA DE ABA com a seta: tres quadros durante o deslize (~80, 180, 330 ms).
  painelTecla(SDLK_UP); painelTecla(SDLK_UP); painelTecla(SDLK_RIGHT);
  quadros(4, NULL);
  snprintf(bmp, sizeof bmp, "%s-1a-troca-80ms.bmp", saida);
  quadros(1, bmp);
  quadros(9, NULL);
  snprintf(bmp, sizeof bmp, "%s-1b-troca-180ms.bmp", saida);
  quadros(1, bmp);
  quadros(14, NULL);
  snprintf(bmp, sizeof bmp, "%s-1c-troca-330ms.bmp", saida);
  quadros(1, bmp);
  quadros(60, NULL);
  spainel_ir_aba(2);                       // Amigos
  { SDL_Event e; memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    spainel_evento(&e); e.key.keysym.sym = SDLK_DOWN; spainel_evento(&e);
    e.key.keysym.sym = SDLK_RETURN; spainel_evento(&e); }   // responde o alcance
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-2-amigos.bmp", saida);
  quadros(1, bmp);
  spainel_ir_aba(4);                       // Agenda
  quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-3-agenda.bmp", saida);
  quadros(1, bmp);
  painelTecla(SDLK_UP);                    // na faixa
  painelTecla(SDLK_RIGHT);                 // lapis
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-4-faixa-lapis.bmp", saida);
  quadros(1, bmp);
  painelTecla(SDLK_RETURN);                // abre Editar
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-5-editar.bmp", saida);
  quadros(1, bmp);
  // Desliga Atividade (linha 1), sobe Agenda ate o topo e confere o arquivo.
  painelTecla(SDLK_DOWN); painelTecla(SDLK_RETURN);
  { int k; painelTecla(SDLK_DOWN); painelTecla(SDLK_DOWN); painelTecla(SDLK_DOWN);
    painelTecla(SDLK_RIGHT); painelTecla(SDLK_LEFT); painelTecla(SDLK_RIGHT);
    for (k = 0; k < 4; k++) painelTecla(SDLK_RETURN); }       // Agenda sobe 4
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-6-editar-mexido.bmp", saida);
  quadros(1, bmp);
  { char *t = dados_ler("salvos-abas-p1.txt");
    printf("arquivo:\n%s\n", t ? t : "(nada)");
    assert(t && strstr(t, "aba\t4\t1\naba\t0\t1"));
    assert(strstr(t, "aba\t1\t0"));
    free(t); }
  painelTecla(SDLK_AC_BACK);               // fecha Editar
  quadros(60, NULL);
  snprintf(bmp, sizeof bmp, "%s-7-depois.bmp", saida);
  quadros(1, bmp);
  // Reabre: a primeira aba e a Agenda (primeira ligada da ordem).
  spainel_fechar(); quadros(40, NULL);
  spainel_abrir(); quadros(90, NULL);
  snprintf(bmp, sizeof bmp, "%s-8-reaberto.bmp", saida);
  quadros(1, bmp);

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
