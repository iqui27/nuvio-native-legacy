// FRIEND PROFILE OF A TRAKT-ONLY FRIEND (M sweep, 04/10/2026), no network.
// Derived from socialui_shot.c; see main() below. Old header follows:
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
#include "../src/recomenda.c"
#include "nvtmp.h"
#include "ajustes.h"
#include "amigoperfil.h"
#include "amigosfil.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "home.h"
#include "layout.h"
#include "salvospainel.h"
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


#include "ilha.h"
#include "rail_shot.h"

// The model state is forced per frame (estado >= 0): there is no server here.
// Run: NUVIO_SHOT_EN=1 NUVIO_SHOT_FONTE=3 NUVIO_SHOT_RELOGIO=1 (NUVIO_SHOT_VIDRO=0
// for the solid material the owner's TV uses), built like socialui_shot.sh.
// One friend that exists only on Trakt ("via Trakt"): the Nuvio server answers
// GET /v1/amigo with 404 for him (rotaAmigo: no `pessoa` row), which the app
// models as REC_SOC_NAO_ACHOU. The same profile is also shot for a Nuvio friend
// whose server profile really is unavailable (NAO_ACHOU too) and for a Nuvio
// friend with data, to prove the three states read differently.
static void quadrosPerfil(int n, const char *bmp, int estado) {
  int i;
  for (i = 0; i < n; i++) {
    Uint32 agora = SDL_GetTicks();
    SDL_PumpEvents();
    if (estado >= 0) { SDL_LockMutex(mtx); temAmigo = 0; amigoEstado = estado; SDL_UnlockMutex(mtx); }
    tex_bombear(8);
    amigoperfil_atualizar(1.0f / 60.0f, agora);
    txt_novo_quadro(); tex_novo_quadro(); gfx_novo_quadro();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    amigoperfil_desenhar(agora);
    rail_shot_relogio();   // NUVIO_SHOT_RELOGIO=1
    if (bmp && i == n - 1) gravarBmp(bmp);
    SDL_GL_SwapWindow(janela);
    SDL_Delay(4);
  }
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nv-amigo/a");
  static CatItem itens[40];
  static CatFileira fils[3];
  SDL_GLContext gl;
  char bmp[800], cache[700];
  int i;
  static SvEvento v[4];
  long long agora = (long long)time(NULL);

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
  janela = SDL_CreateWindow("Nuvio: amigo", 0, 0, 1920, 1080, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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
  memset(itens, 0, sizeof itens);
  memset(fils, 0, sizeof fils);
  for (i = 0; i < 4; i++) {
    CatItem *c = &itens[i];
    snprintf(c->imdb, sizeof c->imdb, "tt90%05d", i);
    snprintf(c->tipo, sizeof c->tipo, "movie");
    snprintf(c->titulo, sizeof c->titulo, "Titulo %d", i);
    snprintf(c->backdrop, sizeof c->backdrop, "deploy/app/art/%02d.jpg", 20 + i);
    snprintf(c->poster, sizeof c->poster, "deploy/app/art/poster/%02d.jpg", 20 + i);
  }
  snprintf(fils[0].chave, sizeof fils[0].chave, "pop_movie");
  snprintf(fils[0].titulo, sizeof fils[0].titulo, "Popular");
  snprintf(fils[0].tipo, sizeof fils[0].tipo, "movie");
  fils[0].ini = 0; fils[0].n = 4;
  cat_definir_tudo(itens, 4, fils, 1);

  // Kevin: Trakt only, watching a series right now (the owner's capture).
  memset(v, 0, sizeof v);
  { SvEvento *e = &v[0];
    snprintf(e->pessoaId, sizeof e->pessoaId, "trakt:kevin");
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "Kevin");
    snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "deploy/app/art/elenco/00_1.jpg");
    e->fonte = SV_FONTE_TRAKT; e->acao = SV_AGORA; e->reacao = SV_REAC_NADA;
    snprintf(e->imdb, sizeof e->imdb, "tt0000104");
    snprintf(e->tipo, sizeof e->tipo, "series");
    snprintf(e->titulo, sizeof e->titulo, "The Gentlemen");
    snprintf(e->poster, sizeof e->poster, "deploy/app/art/poster/09.jpg");
    snprintf(e->arte, sizeof e->arte, "deploy/app/art/09.jpg");
    e->temporada = 2; e->episodio = 5; e->pct = 96; e->quando = agora - 600; }
  // A Nuvio friend whose profile the server refuses to show (blocked / gone).
  { SvEvento *e = &v[1];
    snprintf(e->pessoaId, sizeof e->pessoaId, "nuvio:lia");
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "Lia");
    e->fonte = SV_FONTE_NUVIO; e->acao = SV_FIM; e->reacao = SV_REAC_GOSTOU;
    snprintf(e->imdb, sizeof e->imdb, "tt0000105");
    snprintf(e->tipo, sizeof e->tipo, "movie");
    snprintf(e->titulo, sizeof e->titulo, "Alien");
    snprintf(e->poster, sizeof e->poster, "deploy/app/art/poster/11.jpg");
    snprintf(e->arte, sizeof e->arte, "deploy/app/art/11.jpg");
    e->quando = agora - 3 * 3600; }
  nContatos = 0;
  contato(nContatos++, "trakt:kevin", "Kevin", "deploy/app/art/elenco/00_1.jpg", "trakt");
  contato(nContatos++, "nuvio:lia", "Lia", "", "nuvio");
  socialvis_definir_feed(v, 2);
  quadros(20, NULL);

  desenho = D_PERFIL;
  amigoperfil_abrir("trakt:kevin");
  quadrosPerfil(300, NULL, REC_SOC_NAO_ACHOU);
  snprintf(bmp, sizeof bmp, "%s-trakt-kevin.bmp", saida);
  quadrosPerfil(1, bmp, REC_SOC_NAO_ACHOU);

  amigoperfil_abrir("nuvio:lia");
  quadrosPerfil(300, NULL, REC_SOC_NAO_ACHOU);
  snprintf(bmp, sizeof bmp, "%s-nuvio-lia-indisponivel.bmp", saida);
  quadrosPerfil(1, bmp, REC_SOC_NAO_ACHOU);

  // A Nuvio friend WITH numbers: the 2x2 grid keeps its place and the long
  // labels ("recommendations watched") wrap instead of being cut.
  { char j[1024];
    long long t = (long long)time(NULL);
    SvEvento w;
    memset(&w, 0, sizeof w);
    snprintf(w.pessoaId, sizeof w.pessoaId, "nuvio:pedro");
    snprintf(w.pessoaNome, sizeof w.pessoaNome, "Pedro");
    snprintf(w.pessoaAvatar, sizeof w.pessoaAvatar, "deploy/app/art/elenco/00_0.jpg");
    w.fonte = SV_FONTE_NUVIO; w.acao = SV_FIM; w.reacao = SV_REAC_GOSTOU;
    snprintf(w.imdb, sizeof w.imdb, "tt0000101");
    snprintf(w.tipo, sizeof w.tipo, "movie");
    snprintf(w.titulo, sizeof w.titulo, "Project Hail Mary");
    snprintf(w.poster, sizeof w.poster, "deploy/app/art/poster/03.jpg");
    w.quando = t - 3 * 3600;
    v[2] = w;
    contato(nContatos++, "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", "nuvio");
    socialvis_definir_feed(v, 3);
    snprintf(j, sizeof j,
      "{\"id\":\"nuvio:pedro\",\"nome\":\"Pedro\",\"grau\":1,\"desde\":%lld,\"origem\":\"codigo\","
      "\"compartilha\":1,\"mes\":{\"mes\":\"2026-10\",\"seg\":111600,\"filmes\":12,\"series\":4},"
      "\"gostou\":[{\"imdb\":\"tt0000101\",\"midia\":\"movie\",\"titulo\":\"Project Hail Mary\","
      "\"poster\":\"deploy/app/art/poster/03.jpg\",\"criado\":%lld}],"
      "\"recs\":[{\"id\":9,\"imdb\":\"tt0000102\",\"tipo\":\"movie\",\"titulo\":\"Duna\","
      "\"poster\":\"deploy/app/art/poster/05.jpg\",\"criado\":%lld,\"estado\":\"reacao\",\"terminou\":1,"
      "\"reacao\":1,\"resposta\":\"\",\"respondido\":%lld}]}",
      t - 60 * 86400, t - 3600, t - 2 * 86400, t - 3600);
    assert(amigoParse(j, &amigo));
    amigoperfil_abrir("nuvio:pedro");
    SDL_LockMutex(mtx); temAmigo = 1; amigoEstado = REC_SOC_OK; SDL_UnlockMutex(mtx);
    quadrosPerfil(200, NULL, -1);
    SDL_LockMutex(mtx); temAmigo = 1; amigoEstado = REC_SOC_OK; SDL_UnlockMutex(mtx);
    snprintf(bmp, sizeof bmp, "%s-nuvio-pedro-numeros.bmp", saida);
    quadrosPerfil(1, bmp, -1); }

  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(janela);
  SDL_Quit();
  return 0;
}
